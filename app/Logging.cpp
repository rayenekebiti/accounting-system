#include "Logging.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QMutex>
#include <QRegularExpression>
#include <QtGlobal>
#include <cstdio>

namespace prodlog {

namespace {
QMutex          g_mutex;
Config          g_cfg;
bool            g_init = false;
QtMessageHandler g_prevHandler = nullptr;

QString filePath() { return g_cfg.logDir + "/occountant.log"; }
QString rolledPath(int n) { return QString("%1/occountant.%2.log").arg(g_cfg.logDir).arg(n); }

// Shift occountant.log -> .1, .1 -> .2, ... dropping the oldest beyond `keep`.
void rotateLocked()
{
    if (!g_init) return;
    QFile::remove(rolledPath(g_cfg.keep));
    for (int n = g_cfg.keep - 1; n >= 1; --n) {
        if (QFileInfo::exists(rolledPath(n))) {
            QFile::remove(rolledPath(n + 1));
            QFile::rename(rolledPath(n), rolledPath(n + 1));
        }
    }
    if (QFileInfo::exists(filePath())) {
        QFile::remove(rolledPath(1));
        QFile::rename(filePath(), rolledPath(1));
    }
}

void writeLocked(Level lvl, const QString& category, const QString& message)
{
    if (!g_init || lvl < g_cfg.minLevel) return;
    if (QFileInfo(filePath()).size() >= g_cfg.maxBytes) rotateLocked();
    QFile f(filePath());
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    const QString line = QString("%1 [%2] %3: %4\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate),
             QString::fromLatin1(levelName(lvl)), category, redact(message));
    f.write(line.toUtf8());
    f.close();
}
} // namespace

const char* levelName(Level l)
{
    switch (l) {
    case Level::Info:     return "INFO";
    case Level::Warning:  return "WARN";
    case Level::Error:    return "ERROR";
    case Level::Critical: return "CRIT";
    }
    return "INFO";
}

void init(const Config& cfg)
{
    QMutexLocker lock(&g_mutex);
    g_cfg = cfg;
    QDir().mkpath(g_cfg.logDir);
    g_init = true;
}

bool isInitialized() { QMutexLocker lock(&g_mutex); return g_init; }

void log(Level lvl, const QString& category, const QString& message)
{
    QMutexLocker lock(&g_mutex);
    writeLocked(lvl, category, message);
}

QString redact(const QString& s)
{
    QString out = s;
    // Currency-shaped values ("$1,234.56", "1234.56", "€ 9.99") — never persist an amount.
    static const QRegularExpression money(
        QStringLiteral(R"(([$€£¥]\s?)?\d{1,3}(?:[,\.]\d{3})*[\.,]\d{2}\b)"));
    out.replace(money, QStringLiteral("<redacted-amount>"));
    // Explicit cents fields some diagnostics carry.
    static const QRegularExpression cents(QStringLiteral(R"((?i)\bcents?\s*[:=]\s*-?\d+)"));
    out.replace(cents, QStringLiteral("cents=<redacted>"));
    return out;
}

static void qtHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    Level lvl = Level::Info;
    switch (type) {
    case QtDebugMsg:    lvl = Level::Info;     break;
    case QtInfoMsg:     lvl = Level::Info;     break;
    case QtWarningMsg:  lvl = Level::Warning;  break;
    case QtCriticalMsg: lvl = Level::Error;    break;
    case QtFatalMsg:    lvl = Level::Critical; break;
    }
    log(lvl, ctx.category ? QString::fromLatin1(ctx.category) : QStringLiteral("qt"), msg);
    if (g_prevHandler) g_prevHandler(type, ctx, msg);   // chain (don't swallow console output)
}

void installQtHandler()
{
    g_prevHandler = qInstallMessageHandler(qtHandler);
}

QString currentLogPath() { return g_init ? filePath() : QString(); }

void rotateNow() { QMutexLocker lock(&g_mutex); rotateLocked(); }

void shutdown()
{
    QMutexLocker lock(&g_mutex);
    if (g_prevHandler) { qInstallMessageHandler(g_prevHandler); g_prevHandler = nullptr; }
    g_init = false;
}

} // namespace prodlog
