#ifndef QUICK_DIAGNOSTICS_H
#define QUICK_DIAGNOSTICS_H

#include <QString>
#include <atomic>

// Centralized runtime warning capture. Installs a Qt message handler that
// classifies every Qt/QML diagnostic (binding, type, translation, focus, layout,
// resource-load), counts it, timestamps it, tags it startup vs runtime, and writes
// one low-noise line to stderr. The goal: the app is diagnosable FROM LOGS ALONE —
// no debugger attach needed.
//
// The handler is a TERMINAL SINK: it writes output directly and must NOT route
// through qCWarning()/qDebug() (that re-enters the handler → infinite recursion).
// It composes with the interaction-test handler, which chains back to this one.
namespace diag {

struct Counters {
    // By severity.
    std::atomic<int> warnings{0};
    std::atomic<int> criticals{0};
    std::atomic<int> fatals{0};
    // By class (a single message bumps exactly one class).
    std::atomic<int> binding{0};       // Unable to assign, binding loop, TypeError, ReferenceError
    std::atomic<int> translation{0};   // missing/!available translation catalogs
    std::atomic<int> focus{0};         // focus / FocusScope warnings
    std::atomic<int> layout{0};        // anchors / Layout conflicts
    std::atomic<int> resource{0};      // qrc/file load failures, module not installed
    std::atomic<int> other{0};         // uncategorized warning+
};

// Install the handler. Call ONCE, as early as possible in main() (before the engine).
void install();

// Flip from startup → runtime phase (call once the first frame is up / before exec()).
void enterRuntimePhase();

// Snapshot counters (thread-safe reads).
const Counters& counters();

// One-line human summary, e.g. "warn=3 crit=0 (binding=1 layout=2)". Used by the
// startup diagnostics line and the endurance harness to measure warning DRIFT.
QString summary();

} // namespace diag

#endif // QUICK_DIAGNOSTICS_H
