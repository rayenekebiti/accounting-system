#ifndef CORE_INVOICE_TOTALS_H
#define CORE_INVOICE_TOTALS_H

#include <vector>
#include "Money.h"
#include "InvoiceLine.h"

// Single source of truth for invoice header totals.
//
// INTEGRITY INVARIANTS (must hold for every invoice we ever write):
//   (1) total    == Σ line.lineTotal            — header total equals the lines
//   (2) subtotal + tax == total                 — the three header fields agree
//
// The naive approach — round each aggregate double independently
// (fromDouble(Σ sub), fromDouble(Σ tax), fromDouble(Σ total)) — violates BOTH:
// per-line rounding accumulates fractional cents that a single end rounding does
// not, and three independent roundings of sub/tax/total need not sum. We instead
// derive every header figure from the already-rounded per-line Money so the
// invariants hold by construction (exact integer-cent arithmetic, no drift).
struct InvoiceTotals {
    Money subtotal;
    Money tax;
    Money total;
};

inline InvoiceTotals computeInvoiceTotals(const std::vector<InvoiceLine>& lines)
{
    InvoiceTotals t;
    for (const InvoiceLine& l : lines) {
        if (l.getIsDeleted()) continue;
        const double qty = l.getQuantityMilliunits() / 1000.0;
        // Pre-tax line amount, rounded to cents the same way the line total is.
        const Money lineSub   = Money::fromDouble(l.getUnitPrice().toDouble() * qty);
        const Money lineTotal = l.getLineTotal();          // already rounded in recompute()
        const Money lineTax   = lineTotal - lineSub;       // derived → sub+tax==total per line
        t.subtotal += lineSub;
        t.tax      += lineTax;
        t.total    += lineTotal;
    }
    return t;
}

#endif // CORE_INVOICE_TOTALS_H
