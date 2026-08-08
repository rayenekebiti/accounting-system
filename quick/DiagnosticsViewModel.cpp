#include "DiagnosticsViewModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "storage/CompatibilityManifest.h"
#include "app/SupportBundle.h"
#include "app/SupportId.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <cstdlib>

static QString humanBytes(qint64 b)
{
    if (b < 1024) return QString("%1 B").arg(b);
    double v = b / 1024.0; const char* u = "KB";
    if (v >= 1024) { v /= 1024.0; u = "MB"; }
    if (v >= 1024) { v /= 1024.0; u = "GB"; }
    return QString("%1 %2").arg(v, 0, 'f', 1).arg(u);
}

DiagnosticsViewModel::DiagnosticsViewModel(QObject* parent)
    : QObject(parent)
{}

QString DiagnosticsViewModel::projectionResult() const
{
    if (!ran_) return tr("Not run yet");
    return projectionOk_ ? tr("verified against history at seq %1").arg(projSeq_)
                         : tr("DRIFT — live projection ≠ history at seq %1").arg(projSeq_);
}

QString DiagnosticsViewModel::replayResult() const
{
    if (!ran_) return tr("Not run yet");
    return replayOk_ ? tr("replay-equivalence held at seq %1").arg(replaySeq_)
                     : tr("Verification failed: %1").arg(replayDetail_);
}

QString DiagnosticsViewModel::supportId() const { return supportid::get(); }

QString DiagnosticsViewModel::exportSupportBundle()
{
    bundleBusy_ = true; emit bundleChanged();

    supportbundle::Params p;
    if (StorageService::instance().isInitialized())
        p.dataDir = QString::fromStdString(StorageService::instance().dataDir());
    p.reason    = QStringLiteral("user");
    p.supportId = supportid::get();
    // Empty outputZip → generate() writes <dataDir>/support/SupportBundle.zip.
    const QString path = supportbundle::generate(p);

    lastBundlePath_ = path;
    bundleBusy_ = false;
    emit bundleChanged();
    return path;
}

QString DiagnosticsViewModel::engineVersion() const { return QStringLiteral("Occountant 1.0"); }

QString DiagnosticsViewModel::compatVersion() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    const auto& g = StorageService::instance().governanceVersions();
    return QStringLiteral("schema %1 · replay %2 · statement %3 · snapshot %4")
        .arg(g.schema).arg(g.replay).arg(g.statement).arg(g.snapshot);
}

QString DiagnosticsViewModel::postingPolicy() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    return QStringLiteral("v%1").arg(StorageService::instance().governanceVersions().postingPolicy);
}

QString DiagnosticsViewModel::compatStatus() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    return QString::fromLatin1(compat::toString(StorageService::instance().compatibilityStatus()));
}

QString DiagnosticsViewModel::snapshotStatus() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    const uint64_t s = StorageService::instance().audit().ledgerSnapshotSeq();
    return s == 0 ? tr("none") : tr("valid (at seq %1)").arg(s);
}

QString DiagnosticsViewModel::databaseSize() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    QDir d(QString::fromStdString(StorageService::instance().dataDir()));
    qint64 total = 0;
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files)) total += fi.size();
    return humanBytes(total);
}

QString DiagnosticsViewModel::eventCount() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    return QString::number(StorageService::instance().auditEventCount());
}

QString DiagnosticsViewModel::currentSeq() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    return QString::number(StorageService::instance().audit().lastSeq());
}

QString DiagnosticsViewModel::accountCount() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    auto& aj = StorageService::instance().audit();
    return tr("%1 accounts · %2 entries").arg(aj.accountCount()).arg(aj.entryCount());
}

QString DiagnosticsViewModel::trialBalance() const
{
    if (!StorageService::instance().isInitialized()) return QStringLiteral("—");
    const int64_t t = StorageService::instance().audit().trialBalanceTotal();
    return t == 0 ? tr("balanced ($0.00)") : QString("$%1").arg(t / 100.0, 0, 'f', 2);
}

bool DiagnosticsViewModel::trialBalanceOk() const
{
    return StorageService::instance().isInitialized()
        && StorageService::instance().audit().trialBalanceTotal() == 0;
}

QString DiagnosticsViewModel::lastBackup() const
{
    if (!StorageService::instance().isInitialized()) return tr("never");
    QDir b(QString::fromStdString(StorageService::instance().dataDir()) + "/backups");
    if (!b.exists()) return tr("never");
    const auto dirs = b.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    if (dirs.isEmpty()) return tr("never");
    return dirs.first().lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

void DiagnosticsViewModel::refresh() { emit changed(); }

void DiagnosticsViewModel::runVerification()
{
    if (!StorageService::instance().isInitialized()) return;
    verifying_ = true; emit verificationChanged();

    auto& storage = StorageService::instance();
    // Non-destructive projection verification (live == replay of history).
    const auto vr = storage.verifyAuditProjection();
    projectionOk_ = vr.ok;
    projSeq_      = vr.seq;

    // Deep replay-equivalence gate (full model + snapshot + trial balance).
    const auto cr = storage.validateCompatibility();
    replayOk_     = cr.ok;
    replaySeq_    = cr.seq;
    replayDetail_ = QString::fromStdString(cr.detail);

    ran_ = true;
    verifying_ = false;
    emit verificationChanged();
}
