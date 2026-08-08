#include "SupportBundle.h"
#include "BuildInfo.h"
#include "MiniZip.h"
#include "StartupDiagnostics.h"
#include "Logging.h"
#include "storage/StorageService.h"
#include "storage/CompatibilityManifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QSysInfo>

namespace {

QByteArray readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray b = f.readAll();
    f.close();
    return b;
}

// A file may only enter the bundle if its name matches one of these operator-facing suffixes.
// This is the guardrail that keeps accounting data (*.dat, compat.manifest, journals) OUT even
// if a caller points crashDir/logDir at the wrong place.
bool suffixAllowed(const QString& name, const QStringList& allowed)
{
    for (const QString& suf : allowed)
        if (name.endsWith(suf, Qt::CaseInsensitive)) return true;
    return false;
}

QString formatCompat(StorageService& s)
{
    const CompatibilityReport r = s.compatibilityReport(/*runValidation*/ true);
    auto yn = [](bool b){ return b ? "yes" : "no"; };
    QString out;
    out += "Occountant — Compatibility Report\n";
    out += QString("versions: schema=%1 replay=%2 postingPolicy=%3 statement=%4 snapshot=%5 eventLog=%6 engineBuild=%7\n")
               .arg(r.versions.schema).arg(r.versions.replay).arg(r.versions.postingPolicy)
               .arg(r.versions.statement).arg(r.versions.snapshot).arg(r.versions.eventLogFormat)
               .arg(r.versions.engineBuild);
    out += QString("classification: %1\n").arg(QString::fromLatin1(compat::toString(r.classification)));
    out += QString("headSeq: %1\n").arg(r.headSeq);
    out += QString("migrations since genesis: %1\n").arg(r.migrationCount);
    out += "guarantees satisfied:\n";
    out += QString("  replay equivalence:     %1\n").arg(yn(r.replayValidated));
    out += QString("  snapshot equivalence:   %1\n").arg(yn(r.snapshotValidated));
    out += QString("  trial balance == 0:     %1\n").arg(yn(r.trialBalanceZero));
    out += QString("  historical determinism: %1\n").arg(yn(r.historicalDeterministic));
    out += QString("  ALL: %1\n").arg(yn(r.guaranteesSatisfied()));
    return out;
}

QString environmentText()
{
    QString out;
    out += "Occountant — Environment\n";
    out += "os: "     + QSysInfo::prettyProductName() + "\n";
    out += "kernel: " + QSysInfo::kernelType() + " " + QSysInfo::kernelVersion() + "\n";
    out += "arch: "   + QSysInfo::currentCpuArchitecture() + "\n";
    out += "abi: "    + QSysInfo::buildAbi() + "\n";
    return out;
}

} // namespace

namespace supportbundle {

QString generate(const Params& p)
{
    const QString crashDir = p.crashDir.isEmpty() ? p.dataDir + "/crash" : p.crashDir;
    const QString logDir   = p.logDir.isEmpty()   ? p.dataDir + "/logs"  : p.logDir;

    MiniZip zip;
    QStringList index;   // sha256  bytes  name — the bundle's self-describing manifest

    auto addEntry = [&](const QString& nameInZip, const QByteArray& data) {
        zip.add(nameInZip, data);
        const QByteArray sha = QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
        index << QString("%1  %2  %3").arg(QString::fromLatin1(sha),
                                           QString::number(data.size()).rightJustified(8),
                                           nameInZip);
    };

    // Copy the newest N allowlisted files from a directory into <prefix>/ inside the zip.
    auto addDir = [&](const QString& dir, const QString& prefix,
                      const QStringList& allowedSuffixes, int maxFiles) {
        QDir d(dir);
        if (!d.exists()) return;
        QFileInfoList files = d.entryInfoList(QDir::Files, QDir::Time);   // newest first
        int taken = 0;
        for (const QFileInfo& fi : files) {
            if (taken >= maxFiles) break;
            if (!suffixAllowed(fi.fileName(), allowedSuffixes)) continue;   // guardrail
            addEntry(prefix + "/" + fi.fileName(), readAll(fi.absoluteFilePath()));
            ++taken;
        }
    };

    // 1) Build provenance — always present.
    addEntry("buildinfo.json", buildinfo::json());

    // 2) Environment snapshot (no accounting values).
    addEntry("environment.txt", environmentText().toUtf8());

    // 3) Engine-derived diagnostics + compatibility (read-only; only if the engine is open).
    if (StorageService::instance().isInitialized()) {
        StorageService& s = StorageService::instance();
        const startupdiag::Health h = startupdiag::collect(s, nullptr, nullptr, nullptr);
        addEntry("startup-diagnostics.txt", startupdiag::format(h).toUtf8());
        addEntry("compatibility-report.txt", formatCompat(s).toUtf8());
    } else {
        addEntry("startup-diagnostics.txt",
                 QByteArray("storage not initialised — diagnostics unavailable\n"));
        addEntry("compatibility-report.txt",
                 QByteArray("storage not initialised — compatibility report unavailable\n"));
    }

    // 4) Rolling operator logs (money already redacted at write time). Allowlist: *.log only.
    addDir(logDir, "logs", { ".log" }, p.maxLogFiles);

    // 5) Crash reports (versions + module names only; no accounting values). Allowlist: *.zip/*.txt.
    addDir(crashDir, "crash", { ".zip", ".txt" }, p.maxCrashReports);

    // 6) The bundle's own manifest (written last; lists everything else with its sha256).
    QByteArray manifest;
    manifest += "Occountant Support Bundle\n";
    if (!p.supportId.isEmpty())
        manifest += "support-id: " + p.supportId.toUtf8() + "\n";
    manifest += "reason: " + p.reason.toUtf8() + "\n";
    manifest += "build: "  + buildinfo::oneLine().toUtf8() + "\n\n";
    manifest += "sha256                                                            bytes  file\n";
    manifest += index.join('\n').toUtf8();
    manifest += "\n";
    zip.add("bundle-manifest.txt", manifest);

    // Resolve the output path: an explicit .zip is used as-is; a directory receives SupportBundle.zip.
    QString out = p.outputZip;
    if (out.isEmpty())
        out = (p.dataDir.isEmpty() ? QDir::currentPath() : p.dataDir) + "/support/SupportBundle.zip";
    else if (!out.endsWith(".zip", Qt::CaseInsensitive))
        out = out + "/SupportBundle.zip";

    QDir().mkpath(QFileInfo(out).absolutePath());
    if (!zip.writeTo(out)) {
        prodlog::error("support", "failed to write support bundle to " + out);
        return {};
    }
    prodlog::info("support", "support bundle written: " + out);
    return out;
}

} // namespace supportbundle
