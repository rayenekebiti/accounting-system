#include "diagnostics.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QMutex>
#include <QtGlobal>

#include <cstdio>
#include <cstring>

namespace diag {
namespace {

Counters         g_c;
QtMessageHandler g_prev = nullptr;       // saved, intentionally NOT chained (we are the sink)
std::atomic<bool> g_runtime{false};
bool             g_verbose = false;

QElapsedTimer    g_clock;
QMutex           g_mutex;                // guards dedup state + stderr writes
QString          g_lastKey;             // last message body (for dedup)
int              g_repeat = 0;           // suppressed repeats of g_lastKey

// Classify a message into exactly one class, bumping its counter. Returns a short
// tag for the log line. Order matters: more specific patterns first.
const char* classify(const QString& m)
{
    if (m.contains(QLatin1String("Translations will not be available")) ||
        m.contains(QLatin1String("catalogs.json")) ||
        m.contains(QLatin1String("No translation"))) {
        g_c.translation.fetch_add(1); return "i18n";
    }
    if (m.contains(QLatin1String("is not installed")) ||
        m.contains(QLatin1String("No such file")) ||
        m.contains(QLatin1String("cannot load")) ||
        m.contains(QLatin1String("Cannot open")) ||
        (m.contains(QLatin1String("qrc:")) && m.contains(QLatin1String("rror")))) {
        g_c.resource.fetch_add(1); return "resource";
    }
    if (m.contains(QLatin1String("Unable to assign")) ||
        m.contains(QLatin1String("Binding loop")) ||
        m.contains(QLatin1String("TypeError")) ||
        m.contains(QLatin1String("ReferenceError")) ||
        m.contains(QLatin1String("is not a function")) ||
        m.contains(QLatin1String("Cannot read property")) ||
        m.contains(QLatin1String("Cannot assign"))) {
        g_c.binding.fetch_add(1); return "binding";
    }
    if (m.contains(QLatin1String("anchors")) ||
        m.contains(QLatin1String("anchor loop")) ||
        m.contains(QLatin1String("managed by a layout")) ||
        m.contains(QLatin1String("Layout"))) {
        g_c.layout.fetch_add(1); return "layout";
    }
    if (m.contains(QLatin1String("FocusScope")) ||
        m.contains(QLatin1String("focus"), Qt::CaseInsensitive)) {
        g_c.focus.fetch_add(1); return "focus";
    }
    g_c.other.fetch_add(1); return "other";
}

const char* sevTag(QtMsgType type)
{
    switch (type) {
    case QtFatalMsg:    return "FATAL";
    case QtCriticalMsg: return "CRIT";
    case QtWarningMsg:  return "warn";
    case QtInfoMsg:     return "info";
    default:            return "dbg";
    }
}

// Emit one formatted line with consecutive-duplicate suppression. Caller holds nothing;
// this takes the mutex.
void emitLine(QtMsgType type, const char* cls, const QString& msg)
{
    const char* phase = g_runtime.load() ? "rt" : "up";
    const double t = g_clock.isValid() ? g_clock.nsecsElapsed() / 1.0e9 : 0.0;

    QMutexLocker lock(&g_mutex);
    if (msg == g_lastKey) { ++g_repeat; return; }      // low-noise: collapse repeats
    if (g_repeat > 0) {
        std::fprintf(stderr, "  … previous line repeated %d more time(s)\n", g_repeat);
        g_repeat = 0;
    }
    g_lastKey = msg;

    const QByteArray line = QStringLiteral("[%1s][%2][%3:%4] %5")
        .arg(t, 0, 'f', 3).arg(phase).arg(sevTag(type)).arg(QString::fromUtf8(cls)).arg(msg).toUtf8();
    std::fputs(line.constData(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

void handler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    // Our own structured diagnostics: print verbatim, ALWAYS, and never count them as
    // defects — counters track UNINTENTIONAL framework/QML warnings (drift), not our logs.
    const bool isAcct = ctx.category && std::strncmp(ctx.category, "acct.", 5) == 0;
    if (isAcct) {
        emitLine(type, ctx.category, msg);
        if (type == QtFatalMsg && g_prev) g_prev(type, ctx, msg);
        return;
    }

    // Framework / QML message: count by severity, classify, gate noise.
    const bool warnPlus = (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg);
    switch (type) {
    case QtWarningMsg:  g_c.warnings.fetch_add(1);  break;
    case QtCriticalMsg: g_c.criticals.fetch_add(1); break;
    case QtFatalMsg:    g_c.fatals.fetch_add(1);    break;
    default: break;
    }
    if (!warnPlus && !g_verbose) return;   // drop debug/info unless verbose

    emitLine(type, warnPlus ? classify(msg) : "info", msg);

    if (type == QtFatalMsg && g_prev) g_prev(type, ctx, msg);  // let Qt abort as usual
}

} // namespace

void install()
{
    g_verbose = qEnvironmentVariableIsSet("ACCT_DIAG_VERBOSE");
    g_clock.start();
    g_prev = qInstallMessageHandler(handler);
}

void enterRuntimePhase() { g_runtime.store(true); }

const Counters& counters() { return g_c; }

QString summary()
{
    return QStringLiteral("warn=%1 crit=%2 fatal=%3 (binding=%4 i18n=%5 focus=%6 layout=%7 resource=%8 other=%9)")
        .arg(g_c.warnings.load()).arg(g_c.criticals.load()).arg(g_c.fatals.load())
        .arg(g_c.binding.load()).arg(g_c.translation.load()).arg(g_c.focus.load())
        .arg(g_c.layout.load()).arg(g_c.resource.load()).arg(g_c.other.load());
}

} // namespace diag
