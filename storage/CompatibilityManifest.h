#ifndef STORAGE_COMPATIBILITY_MANIFEST_H
#define STORAGE_COMPATIBILITY_MANIFEST_H

#include <cstdint>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Deterministic Historical Compatibility & Evolution Governance — the version
// contract for evolving the engine WITHOUT silently changing the accounting
// meaning of existing books.
//
// The problem this closes: the core is feature-complete, but future releases may
// change schemas, the replay implementation, posting policy, statement derivation,
// or the snapshot format. Historical books must still reconstruct to the SAME
// accounting meaning (balances, settlement, trial balance, statements) — not merely
// the same records. To guarantee that, every compatibility axis is an explicit,
// independent VERSION, and the boundaries between "compatible", "migration-required",
// and "incompatible" are declared, not implicit.
//
// Authority model (consistent with the rest of the engine): the AUTHORITATIVE record
// of which versions authored a history is an `EngineVersionStamp` event in the log.
// The on-disk `compat.manifest` file is a disposable, rebuildable PROJECTION of that —
// a fast, operator-visible read of the current contract. A missing or corrupt manifest
// is never fatal: it is rebuilt from the stamp events (the log is truth).
// ─────────────────────────────────────────────────────────────────────────────

// The governance version vector. Each field is an INDEPENDENT compatibility axis — a
// build may evolve one without the others. A field value of 0 means "unset / authored
// by a pre-governance build" and is read as the baseline (equivalent to 1), never as a
// downgrade. NEVER reorder these fields — they are serialized positionally into the
// EngineVersionStamp payload and the manifest, both of which are on disk forever.
struct GovernanceVersions {
    uint16_t schema         = 0;   // aggregate engine record-schema generation (per-file layout stays in BinaryRecordFile)
    uint16_t replay         = 0;   // AuditJournal::apply() event-interpretation semantics
    uint16_t postingPolicy  = 0;   // operational-fact → ledger-posting mapping (PostingPolicy)
    uint16_t statement      = 0;   // financial-statement derivation
    uint16_t snapshot       = 0;   // ledger-snapshot on-disk format
    uint16_t eventLogFormat = 0;   // EventLog container/frame format
    uint16_t engineBuild    = 0;   // informational build stamp (never gates compatibility)

    bool operator==(const GovernanceVersions&) const = default;
};

namespace compat {

// The versions THIS build implements. Bumping one of these is the explicit, auditable
// act of declaring an evolution — and obligates a matching entry in the semantic-
// migration registry and the replay-equivalence gate (see docs/compatibility-governance.md).
constexpr uint16_t kSchemaVersion         = 1;
constexpr uint16_t kReplayVersion         = 1;
constexpr uint16_t kPostingPolicyVersion  = 2;   // v2: invoice/expense tax split (Tax Payable / Recoverable Tax)
constexpr uint16_t kStatementVersion      = 1;
constexpr uint16_t kSnapshotVersion       = 1;
constexpr uint16_t kEventLogFormatVersion = 1;
constexpr uint16_t kEngineBuild           = 1;

// The OLDEST version of each axis this build can still read (forward-migrate from).
// Books authored below a floor cannot be safely reinterpreted → refused, not corrupted.
constexpr uint16_t kMinSchema         = 1;
constexpr uint16_t kMinReplay         = 1;
constexpr uint16_t kMinPostingPolicy  = 1;
constexpr uint16_t kMinStatement      = 1;
constexpr uint16_t kMinSnapshot       = 1;
constexpr uint16_t kMinEventLogFormat = 1;

// This build's version vector.
GovernanceVersions current();

// This build's per-axis floors (the oldest it can migrate forward from).
GovernanceVersions floors();

enum class Compatibility {
    Compatible,        // on-disk == code (per axis) → open directly
    MigrationRequired, // on-disk < code on some axis (and ≥ floor) → forward-migrate then open
    Incompatible,      // on-disk > code (downgrade) OR below a floor → REFUSE to open, loudly
};

// Classify on-disk governance against an arbitrary `code` vector (the 3-arg form is the
// testable core; the 2-arg form uses current()). A precise human-readable explanation is
// written to `reason` when the result is not Compatible. Semantics per axis:
//   onDisk > code                → Incompatible (a newer build wrote these books; downgrade)
//   onDisk != 0 && onDisk < floor→ Incompatible (too old to reinterpret safely)
//   onDisk != 0 && onDisk < code → MigrationRequired
//   otherwise (==, or 0/unset)   → Compatible
// Whether a MigrationRequired axis actually HAS a registered migration path is decided by
// the caller against the SemanticMigration registry; a missing path is downgraded to
// Incompatible there.
Compatibility classify(const GovernanceVersions& onDisk,
                       const GovernanceVersions& code,
                       const GovernanceVersions& floors,
                       std::string& reason);
Compatibility classify(const GovernanceVersions& onDisk, std::string& reason);

const char* toString(Compatibility c);

} // namespace compat

// The operator-visible compatibility report (deliverable 8). Assembled by StorageService
// from the governance events + the replay-equivalence validator; formatted for the
// `acct.compat` startup line and the ACCT_COMPAT_REPORT headless dump.
struct CompatibilityReport {
    GovernanceVersions      versions;                       // current authoritative contract
    compat::Compatibility   classification = compat::Compatibility::Compatible;
    std::size_t             migrationCount = 0;             // version transitions after genesis
    bool                    replayValidated   = false;      // genesis replay-equivalence held
    bool                    snapshotValidated = false;      // snapshot == genesis (or none present)
    bool                    trialBalanceZero  = false;      // ledger invariant
    bool                    historicalDeterministic = false;// reconstruction is reproducible
    bool                    validationRun = false;          // was the deep validator run?
    uint64_t                headSeq = 0;                    // history head the report covers

    // All the "historical guarantees satisfied" flags, ANDed. Only meaningful when
    // validationRun is true.
    bool guaranteesSatisfied() const {
        return replayValidated && snapshotValidated && trialBalanceZero && historicalDeterministic;
    }
};

// Crash-safe on-disk projection of the governance vector: <dataDir>/compat.manifest.
// Layout (36 bytes, fixed):
//   [ 0.. 7] char[8]  magic "ACCTMFST"
//   [ 8.. 9] uint16_t fileFormatVersion (1)
//   [10..11] uint16_t schema
//   [12..13] uint16_t replay
//   [14..15] uint16_t postingPolicy
//   [16..17] uint16_t statement
//   [18..19] uint16_t snapshot
//   [20..21] uint16_t eventLogFormat
//   [22..23] uint16_t engineBuild
//   [24..31] reserved (0)
//   [32..35] uint32_t crc32 over bytes [0..31]
class CompatibilityManifest {
public:
    static constexpr std::size_t kSize = 36;

    // Read + integrity-check the manifest. Returns false if absent, short, wrong magic,
    // wrong file-format, or CRC-mismatch — in every "false" case the caller rebuilds the
    // projection from the authoritative EngineVersionStamp events (never fatal).
    static bool read(const std::string& path, GovernanceVersions& out);

    // Write atomically: temp → fflush → fsync → rename over the manifest. A crash leaves
    // the OLD manifest (or none) — never a partial one. Honors ACCT_CRASH_POINT
    // "afterManifestTmp" (hard-exit after the durable temp, before the install) so the
    // cross-process crash test can prove complete-or-absent. Throws on hard I/O failure.
    static void write(const std::string& path, const GovernanceVersions& v);
};

#endif // STORAGE_COMPATIBILITY_MANIFEST_H
