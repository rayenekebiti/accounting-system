#ifndef APP_CRASH_REPORTER_H
#define APP_CRASH_REPORTER_H

#include <QString>

// Local-first crash reporting. On a fatal signal / std::terminate it collects a stack trace,
// build + governance versions, platform, and loaded modules into a single CrashReport_<ts>.zip
// under the report dir. NOTHING is ever transmitted — the user explicitly chooses whether to send
// the file. No telemetry, no analytics, no accounting VALUES (only versions + module names).
namespace crashreport {

struct Context {
    QString buildVersion;   // appinfo::fullVersion()
    QString buildId;
    QString channel;
    QString platform;       // OS + arch
    QString governance;     // engine version-contract line (read-only, from StorageService)
    QString reportDir;      // where CrashReport_*.zip is written
};

// Capture the context + install signal/terminate handlers. Call once, after the engine has
// initialised (so governance versions are known).
void install(const Context& ctx);

// Build a report now and return its .zip path (also the path used by the crash handlers). Safe to
// call directly — the deterministic regression test uses this rather than provoking a real crash.
QString generate(const QString& reason, const QString& stackText = QString());

// Best-effort human-readable backtrace of the current thread (module+offset frames).
QString captureStack();

QString lastReportPath();

} // namespace crashreport

#endif // APP_CRASH_REPORTER_H
