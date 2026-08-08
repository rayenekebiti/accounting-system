#ifndef APP_LOGGING_H
#define APP_LOGGING_H

#include <QString>

// Production logging for the commercial layer: four levels, size-based rolling files, and
// defensive redaction so sensitive accounting VALUES (money amounts) are never written to disk.
// This is distinct from the dev-facing qDebug/diagnostics pipeline — it is the operator/support
// log that ships with the product. Nothing here is on the accounting write path.
namespace prodlog {

enum class Level { Info = 0, Warning = 1, Error = 2, Critical = 3 };

struct Config {
    QString logDir;                 // <dataDir>/logs
    qint64  maxBytes = 1 << 20;     // rotate at 1 MiB
    int     keep     = 5;           // occountant.1.log .. occountant.5.log
    Level   minLevel = Level::Info;
};

void init(const Config& cfg);       // create logDir + open the current file (idempotent)
bool isInitialized();
void log(Level lvl, const QString& category, const QString& message);
inline void info    (const QString& c, const QString& m) { log(Level::Info,     c, m); }
inline void warning (const QString& c, const QString& m) { log(Level::Warning,  c, m); }
inline void error   (const QString& c, const QString& m) { log(Level::Error,    c, m); }
inline void critical(const QString& c, const QString& m) { log(Level::Critical, c, m); }

// Chain Qt's qDebug/qWarning/... into the rolling file (redacted). Preserves any prior handler.
void installQtHandler();

// Mask currency-like tokens so a stray value can never leak into a shipped log.
QString redact(const QString& s);

QString  currentLogPath();
void     rotateNow();               // force a rotation (used by the rotation regression test)
void     shutdown();

const char* levelName(Level l);

} // namespace prodlog

#endif // APP_LOGGING_H
