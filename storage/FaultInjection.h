#ifndef STORAGE_FAULT_INJECTION_H
#define STORAGE_FAULT_INJECTION_H

#include <cstdlib>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Deterministic fault injection for the persistence abstraction WE own.
//
// Sibling to the crash hooks (evtMaybeCrash / ajMaybeCrash / cmMaybeCrash) which model
// abrupt process death. This models a persistence WRITE failing at a named point so the
// operation takes its existing error path — used to prove OUR recovery logic (reconcile /
// uncommitted-tail truncation / manifest rebuild / snapshot genesis-fallback) on the next
// open. It is NOT an attempt to emulate arbitrary OS faults (malloc/read/open interception);
// the hooks live only at the handful of write points we control.
//
// Inert unless ACCT_FAULT_POINT names the point. When it does, the call site forces its
// failure path (a loud throw with a diagnostic) exactly once for that run.
//
//   Points:
//     logCommit     — EventLog::writeFileHeader (the commit-point header write)
//     cursorWrite   — AuditJournal::writeCursor (projection cursor advance)
//     snapshotWrite — AuditJournal::writeLedgerSnapshot (ledger snapshot install)
//     manifestWrite — CompatibilityManifest::write (compat manifest projection)
// ─────────────────────────────────────────────────────────────────────────────
// In-process ONE-SHOT armed fault: fires exactly on the next matching call, then clears.
// Lets a test build valid committed state first, then arm the precise target write (the
// env path below fires on EVERY match, which trips the empty-log creation — not useful for
// logCommit/cursorWrite which write a header every time).
inline const char*& acctArmedFault() { static const char* p = nullptr; return p; }
inline void acctArmFault(const char* point) { acctArmedFault() = point; }

inline bool acctFaultAt(const char* point)
{
    if (!point) return false;
    const char*& armed = acctArmedFault();
    if (armed && std::strcmp(armed, point) == 0) { armed = nullptr; return true; }   // fire once
    const char* want = std::getenv("ACCT_FAULT_POINT");                              // env: fire every match
    return want && std::strcmp(want, point) == 0;
}

#endif // STORAGE_FAULT_INJECTION_H
