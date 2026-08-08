#include "CrashReporter.h"
#include "MiniZip.h"
#include "Logging.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QSysInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <csignal>
#include <cstdlib>
#include <exception>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <psapi.h>
#endif

namespace crashreport {
namespace {
Context g_ctx;
QString  g_last;
bool     g_installed = false;

QStringList loadedModules()
{
    QStringList mods;
#ifdef Q_OS_WIN
    HMODULE handles[512];
    DWORD needed = 0;
    if (EnumProcessModules(GetCurrentProcess(), handles, sizeof(handles), &needed)) {
        const int n = int(needed / sizeof(HMODULE));
        for (int i = 0; i < n && i < 512; ++i) {
            char name[MAX_PATH];
            if (GetModuleBaseNameA(GetCurrentProcess(), handles[i], name, MAX_PATH))
                mods << QString::fromLatin1(name);
        }
    }
#endif
    return mods;
}

void handleSignal(int sig)
{
    // Best-effort: build a report, then re-raise the default handler so the OS still records it.
    generate(QString("fatal signal %1").arg(sig), captureStack());
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

void handleTerminate()
{
    generate(QStringLiteral("std::terminate (unhandled exception)"), captureStack());
    std::abort();
}
} // namespace

QString captureStack()
{
    QString out;
#ifdef Q_OS_WIN
    void* frames[64];
    const USHORT n = RtlCaptureStackBackTrace(0, 64, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        HMODULE mod = nullptr;
        char base[MAX_PATH] = {0};
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(frames[i]), &mod)) {
            GetModuleBaseNameA(GetCurrentProcess(), mod, base, MAX_PATH);
            const quintptr off = reinterpret_cast<quintptr>(frames[i]) - reinterpret_cast<quintptr>(mod);
            out += QString("  #%1  %2+0x%3\n").arg(i, 2).arg(QString::fromLatin1(base)).arg(off, 0, 16);
        } else {
            out += QString("  #%1  0x%2\n").arg(i, 2).arg(reinterpret_cast<quintptr>(frames[i]), 0, 16);
        }
    }
#endif
    if (out.isEmpty()) out = QStringLiteral("  (stack capture unavailable on this platform)\n");
    return out;
}

void install(const Context& ctx)
{
    g_ctx = ctx;
    if (g_installed) return;
    g_installed = true;
    std::signal(SIGSEGV, handleSignal);
    std::signal(SIGABRT, handleSignal);
    std::signal(SIGILL,  handleSignal);
    std::signal(SIGFPE,  handleSignal);
    std::set_terminate(handleTerminate);
    prodlog::info("crash", "crash reporter armed");
}

QString generate(const QString& reason, const QString& stackText)
{
    const qint64 ts = QDateTime::currentSecsSinceEpoch();
    const QString zipPath = g_ctx.reportDir + QString("/CrashReport_%1.zip").arg(ts);

    const QString platform = g_ctx.platform.isEmpty()
        ? (QSysInfo::prettyProductName() + " / " + QSysInfo::currentCpuArchitecture())
        : g_ctx.platform;
    const QStringList mods = loadedModules();

    // Human-readable report.
    QString report;
    report += "Occountant Crash Report\n=======================\n";
    report += "when:        " + QDateTime::fromSecsSinceEpoch(ts).toUTC().toString(Qt::ISODate) + "Z\n";
    report += "reason:      " + reason + "\n";
    report += "build:       " + g_ctx.buildVersion + "\n";
    report += "buildId:     " + g_ctx.buildId + "\n";
    report += "channel:     " + g_ctx.channel + "\n";
    report += "platform:    " + platform + "\n";
    report += "governance:  " + g_ctx.governance + "\n";
    report += "modules (" + QString::number(mods.size()) + "):\n";
    for (const QString& m : mods) report += "  - " + m + "\n";
    report += "\nstack:\n" + stackText;
    report += "\nNote: this file contains NO accounting data — only versions, platform, and module names.\n";

    // Structured manifest (for a future, user-initiated upload path).
    QJsonObject o;
    o["reason"]     = reason;
    o["build"]      = g_ctx.buildVersion;
    o["buildId"]    = g_ctx.buildId;
    o["channel"]    = g_ctx.channel;
    o["platform"]   = platform;
    o["governance"] = g_ctx.governance;
    o["when"]       = double(ts);
    QJsonArray ma; for (const QString& m : mods) ma.append(m);
    o["modules"]    = ma;

    MiniZip zip;
    zip.add("report.txt",   report.toUtf8());
    zip.add("manifest.json", QJsonDocument(o).toJson(QJsonDocument::Indented));
    // Include a redacted tail of the production log if present (bounded).
    const QString logPath = prodlog::currentLogPath();
    if (!logPath.isEmpty() && QFileInfo::exists(logPath)) {
        QFile lf(logPath);
        if (lf.open(QIODevice::ReadOnly)) {
            QByteArray tail = lf.readAll();
            if (tail.size() > 64 * 1024) tail = tail.right(64 * 1024);
            zip.add("occountant.log", tail);
            lf.close();
        }
    }
    zip.writeTo(zipPath);
    g_last = zipPath;
    return zipPath;
}

QString lastReportPath() { return g_last; }

} // namespace crashreport
