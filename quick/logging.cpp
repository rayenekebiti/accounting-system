#include "logging.h"

// Default levels chosen to be quiet in steady state but informative for the
// data-reliability path. Override with QT_LOGGING_RULES at runtime.
Q_LOGGING_CATEGORY(lcStorage,     "acct.storage",     QtWarningMsg)
Q_LOGGING_CATEGORY(lcPersistence, "acct.persistence", QtWarningMsg)
Q_LOGGING_CATEGORY(lcRecovery,    "acct.recovery",    QtInfoMsg)
Q_LOGGING_CATEGORY(lcIntegrity,   "acct.integrity",   QtInfoMsg)
Q_LOGGING_CATEGORY(lcStartup,     "acct.startup",     QtInfoMsg)
