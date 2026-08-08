#ifndef APP_SUPPORT_BUNDLE_H
#define APP_SUPPORT_BUNDLE_H

#include <QString>

// Support bundle: one shareable SupportBundle.zip a user can hand to support, composed ONLY from
// the operator-facing signals — build provenance, rolling logs (money already redacted at write
// time), crash reports (versions + module names, no values), startup diagnostics, and the
// compatibility report. It is assembled by allowlist, so no *.dat / customer / invoice / payment /
// ledger byte ever enters it. Nothing is transmitted — the user chooses whether to send the file.
// Deterministic + offline; sits entirely above the accounting engine (reads it, never mutates it).
namespace supportbundle {

struct Params {
    QString outputZip;              // exact .zip path, or a directory (SupportBundle.zip is placed in it)
    QString dataDir;                // app data dir (used to default crashDir/logDir)
    QString crashDir;               // default: <dataDir>/crash
    QString logDir;                 // default: <dataDir>/logs
    QString reason = QStringLiteral("user");
    QString supportId;              // non-PII install id (supportid::get); recorded in the manifest
    int     maxCrashReports = 10;   // newest N
    int     maxLogFiles     = 10;   // newest N
};

// Returns the written .zip path, or an empty string on failure. Guarantees no accounting data.
QString generate(const Params& p);

} // namespace supportbundle

#endif // APP_SUPPORT_BUNDLE_H
