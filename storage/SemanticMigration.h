#ifndef STORAGE_SEMANTIC_MIGRATION_H
#define STORAGE_SEMANTIC_MIGRATION_H

#include <cstdint>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Semantic-migration governance — the THREE DISTINCT migration tiers, kept as
// separate concepts (conflating them is how historical meaning gets corrupted):
//
//   Tier::Binary      — byte-level re-encoding of a field, SAME meaning (e.g. money
//                       double → int64 cents). Owned by MigrationV1. Not a schema or
//                       meaning change; the record still says the same thing.
//   Tier::Schema      — record-LAYOUT change (add/reinterpret a field). Owned by
//                       BinaryRecordFile (forward-only, atomic, downgrade-refused).
//                       Changes storage shape, not accounting interpretation.
//   Tier::Accounting  — a change to how EVENTS are INTERPRETED (posting policy,
//                       statement derivation, replay). This registry. The governing
//                       rule: an accounting-semantic migration may only add
//                       interpretation for NEW events and must leave every
//                       pre-migration balance / statement / settlement UNCHANGED. The
//                       replay-equivalence gate reconstructs the books pre- and
//                       post-migration and REFUSES if any historical value moved.
//
// v1 ships this machinery with an EMPTY registry (no accounting-semantic migration has
// been needed yet). The first real one bumps the matching compat-manifest axis and adds
// a Migration entry here, gated by the equivalence proof.
// ─────────────────────────────────────────────────────────────────────────────
namespace semantic {

enum class Tier { Binary, Schema, Accounting };

// A registered accounting-semantic migration advancing one governance axis by one step.
// `axis` names the GovernanceVersions field (e.g. "postingPolicy", "statement", "replay").
struct Migration {
    Tier        tier = Tier::Accounting;
    std::string axis;
    uint16_t    from = 0;
    uint16_t    to   = 0;
};

// The authoritative registry of accounting-semantic migrations this build ships.
// Single definition, safe to include widely.
//
// postingPolicy 1→2 (Tax Engine): v2 splits invoice/expense tax into Tax Payable / Recoverable
// Tax. This is a NO-OP for historical data: every pre-v2 posting is a persisted
// JournalEntryPosted event and replays byte-for-byte unchanged; v2 only maps NEW facts. The
// replay-equivalence gate reconstructs the books and refuses if any historical balance moved.
inline const std::vector<Migration>& registry()
{
    static const std::vector<Migration> kRegistry{
        Migration{ Tier::Accounting, "postingPolicy", 1, 2 },
    };
    return kRegistry;
}

// Is there a registered accounting-semantic migration advancing `axis` from `from` to
// `to`? A MigrationRequired axis with NO registered path must be treated as Incompatible
// (refuse) — the engine never guesses at reinterpreting history.
inline bool hasPath(const std::string& axis, uint16_t from, uint16_t to)
{
    for (const Migration& m : registry())
        if (m.axis == axis && m.from == from && m.to == to)
            return true;
    return false;
}

} // namespace semantic

#endif // STORAGE_SEMANTIC_MIGRATION_H
