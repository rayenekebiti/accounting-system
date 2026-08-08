#ifndef STORAGE_POSTING_POLICY_H
#define STORAGE_POSTING_POLICY_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Versioned posting policy — the operational-fact → balanced-ledger-postings mapping.
//
// The policy is C++ CODE, not data (no DSL / rules engine / plugins — explicitly out of
// scope). It stays FIXED and inspectable; versioning it does NOT make it configurable.
// The version exists so that:
//   • historical postings are attributable to the policy revision that authored them, and
//   • a future policy V2 can map NEW operational facts differently WITHOUT ever touching
//     the historical postings authored by V1 — those are persisted `JournalEntryPosted`
//     events and replay unchanged. Meaning is never recomputed from a mutable policy.
//
// This is the seam that makes "posting-policy evolution" deterministic: adding a V2 here
// obligates a compatibility-manifest `postingPolicy` bump + a replay-equivalence proof
// that no pre-V2 balance/statement changes (see docs/compatibility-governance.md).
// ─────────────────────────────────────────────────────────────────────────────
namespace posting {

// One signed posting: DEBIT positive, CREDIT negative. Layout matches
// AuditJournal::PostingInput field-for-field (converted at the call boundary).
struct Line {
    uint32_t accountId   = 0;
    int64_t  amountCents = 0;
};

// The role-bound account ids the policy posts against. Bound once at runtime
// (AuditJournal::setPostingAccounts); the LEDGER's determinism comes from the persisted
// postings, not from this binding.
struct Roles {
    uint32_t receivable     = 0xFFFFFFFFu;
    uint32_t revenue        = 0xFFFFFFFFu;
    uint32_t cash           = 0xFFFFFFFFu;
    uint32_t expense        = 0xFFFFFFFFu;   // the single "Expenses" account (all expenses debit it)
    uint32_t payable        = 0xFFFFFFFFu;   // "Accounts Payable" (credit purchases)
    uint32_t taxPayable     = 0xFFFFFFFFu;   // "Tax Payable" (output tax collected on sales)
    uint32_t recoverableTax = 0xFFFFFFFFu;   // "Recoverable Tax" (input tax paid on purchases)
};

// The policy version this build authors NEW postings with.
//   V1 — invoice: Dr AR / Cr Revenue (tax lumped into revenue). Expense: Dr Expense / Cr Cash|AP.
//   V2 — invoice: Dr AR / Cr Revenue (net) / Cr Tax Payable (tax). Expense: Dr Expense (net) /
//        Dr Recoverable Tax / Cr Cash|AP. Historical V1 postings replay UNCHANGED (they are
//        persisted events, not policy calls) — V2 only maps NEW facts. Bumping this obligates a
//        registered semantic-migration (postingPolicy 1→2) + the replay-equivalence gate.
constexpr uint16_t kCurrentPostingPolicyVersion = 2;

// Is `ver` a policy revision this build can author with? (Historical postings of ANY
// past version replay unchanged regardless — they are events, not policy calls.)
inline bool isSupported(uint16_t ver) { return ver == 1 || ver == 2; }

// Invoice recognises revenue and a receivable — Dr AR / Cr Revenue. Tax-unaware primitive,
// valid in both v1 and v2 (the v2 tax split lives in invoiceRevenueTaxed).
inline std::vector<Line> invoiceRevenue(uint16_t ver, int64_t totalCents, const Roles& r)
{
    switch (ver) {
    case 1:
    case 2:
        return { Line{ r.receivable,  totalCents },
                 Line{ r.revenue,    -totalCents } };
    default:
        throw std::runtime_error("posting: unsupported posting-policy version "
                                 + std::to_string(ver));
    }
}

// Receiving a payment clears the receivable into cash — Dr Cash / Cr AR (v1 and v2).
inline std::vector<Line> paymentReceipt(uint16_t ver, int64_t amountCents, const Roles& r)
{
    switch (ver) {
    case 1:
    case 2:
        return { Line{ r.cash,        amountCents },
                 Line{ r.receivable, -amountCents } };
    default:
        throw std::runtime_error("posting: unsupported posting-policy version "
                                 + std::to_string(ver));
    }
}

// Recording an expense — Dr Expenses / Cr (Cash for an immediate payment, or Accounts
// Payable for a credit purchase). Balanced by construction (Σ == 0).
inline std::vector<Line> expensePosting(uint16_t ver, int64_t amountCents, bool onCredit, const Roles& r)
{
    switch (ver) {
    case 1:
    case 2:
        return { Line{ r.expense,                        amountCents },
                 Line{ onCredit ? r.payable : r.cash,   -amountCents } };
    default:
        throw std::runtime_error("posting: unsupported posting-policy version "
                                 + std::to_string(ver));
    }
}

// V2 — invoice with an explicit tax split: Dr AR (net+tax) / Cr Revenue (net) / Cr Tax Payable
// (tax). The tax line is omitted when taxCents == 0, so a zero-rated/exempt invoice degenerates
// to the untaxed posting. Balanced by construction (AR debit == Σ credits).
inline std::vector<Line> invoiceRevenueTaxed(uint16_t ver, int64_t netCents, int64_t taxCents, const Roles& r)
{
    switch (ver) {
    case 2: {
        std::vector<Line> out;
        out.push_back(Line{ r.receivable, netCents + taxCents });
        out.push_back(Line{ r.revenue,   -netCents });
        if (taxCents != 0) out.push_back(Line{ r.taxPayable, -taxCents });
        return out;
    }
    default:
        throw std::runtime_error("posting: invoiceRevenueTaxed requires posting-policy v2 (got "
                                 + std::to_string(ver) + ")");
    }
}

// V2 — expense with recoverable input tax: Dr Expense (net) / Dr Recoverable Tax (tax) /
// Cr Cash|AP (net+tax). The recoverable line is omitted when taxCents == 0. Balanced.
inline std::vector<Line> expenseTaxed(uint16_t ver, int64_t netCents, int64_t taxCents,
                                      bool onCredit, const Roles& r)
{
    switch (ver) {
    case 2: {
        std::vector<Line> out;
        out.push_back(Line{ r.expense, netCents });
        if (taxCents != 0) out.push_back(Line{ r.recoverableTax, taxCents });
        out.push_back(Line{ onCredit ? r.payable : r.cash, -(netCents + taxCents) });
        return out;
    }
    default:
        throw std::runtime_error("posting: expenseTaxed requires posting-policy v2 (got "
                                 + std::to_string(ver) + ")");
    }
}

} // namespace posting

#endif // STORAGE_POSTING_POLICY_H
