#ifndef QUICK_LOGGING_H
#define QUICK_LOGGING_H

#include <QLoggingCategory>

// Structured logging categories. Keep diagnostics OFF the user-facing path: these
// go to stderr / the Qt log, NOT to UI. Enable selectively via QT_LOGGING_RULES,
// e.g.  QT_LOGGING_RULES="acct.persistence=true;acct.recovery=true".
//
//   acct.startup      — one-shot environment/health line at launch
//   acct.storage      — storage open/close, lock acquisition, paths
//   acct.persistence  — save/update/commit errors (the data-loss-critical path)
//   acct.recovery     — journal replay / crash-recovery reporting at startup
//   acct.integrity    — accounting-invariant assertion failures (totals, refs)
//
// Categories are info-enabled by default for startup/recovery/integrity (rare,
// important), and warning-only for storage/persistence (avoid steady-state spam).
Q_DECLARE_LOGGING_CATEGORY(lcStorage)
Q_DECLARE_LOGGING_CATEGORY(lcPersistence)
Q_DECLARE_LOGGING_CATEGORY(lcRecovery)
Q_DECLARE_LOGGING_CATEGORY(lcIntegrity)
Q_DECLARE_LOGGING_CATEGORY(lcStartup)

#endif // QUICK_LOGGING_H
