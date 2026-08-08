# Invoices Screen — Reference Implementation Spec

The first production-quality screen. It is the **template** every future screen copies:
its layout rhythm, state handling, RTL behavior, and interaction philosophy become the
house style. Built on the `App` component module. Read-only data from the existing C++
core (no editing yet — that is a later phase).

## Persona & philosophy
Primary user = **non-technical small-business owner**. Therefore:
- Friendly **card list**, not a dense spreadsheet.
- Plain language ("Paid", "Awaiting payment"), never accounting jargon.
- The screen must be calm: generous whitespace, one clear primary action, no visual overload.
- Every state (loading / empty / filtered-empty / populated) is designed, not incidental.

## Layout (top → bottom)
A single column, max content width comfortable for reading, page padding `Theme.space.xl`.

1. **PageHeader**
   - title: "Invoices"
   - subtitle: a live summary, e.g. "12 invoices · 3 awaiting payment" (computed).
   - actions slot: a primary `AppButton { text: "New Invoice" }` (variant primary).
     For now it opens nothing destructive — wire it to a `newInvoiceRequested()` signal
     that just logs / shows a placeholder (editing comes later).

2. **Summary StatCards row** (3 cards, equal width, `Theme.space.md` gap):
   - "Outstanding" — sum of Posted+Overdue totals — tone default (it is a receivable, not
     a gain/loss, so NOT income-green; keep neutral/brand).
   - "Overdue" — sum of Overdue totals — tone "pending" if > 0 else neutral.
   - "Paid (this period)" — sum of Paid totals — tone "income".
   - Each card caption shows the count, e.g. "4 invoices".

3. **FilterBar**
   - SearchField (placeholder "Search invoices…") filtering by invoice number OR customer
     name (case-insensitive substring).
   - A row of status filter `Chip`s: All · Draft · Posted · Paid · Overdue · Void.
     Exactly one selected at a time (default "All"). Selecting filters the list.

4. **The list**
   - A `ListView` of `InvoiceRow`-style `ListRowCard`s, vertical, `Theme.space.sm` spacing,
     smooth wheel scrolling, virtualized.
   - Each row (reuse/extend the existing InvoiceRow look):
     - leading `Avatar { name: customer }`
     - invoice number (bold) + customer name (muted) stacked
     - flexible spacer
     - issue date (muted, small) — hidden on very narrow widths is fine, optional
     - `CurrencyAmount { amount: totalText }` (sign "none" — invoices are receivables)
     - `StatusBadge { status }`
   - Row hover highlights (already in ListRowCard); click emits a `rowActivated(int row)`
     signal (logs for now — opening the editor is a later phase).

5. **States** (mutually exclusive, the list area swaps between them):
   - **Loading**: a `BusyIndicator` centered (only briefly; data is local/fast — acceptable
     to skip if load is synchronous, but the state must exist for the future async path).
   - **Empty (no invoices at all)**: `EmptyState` with icon "🧾", title "No invoices yet",
     description "Create your first invoice to start tracking what you're owed.",
     actionText "New Invoice" → same as header action.
   - **Filtered-empty (search/filter yields nothing)**: `EmptyState` with icon "🔍",
     title "No matches", description "Try a different search or filter.", NO action button.
   - **Populated**: the list.

## RTL (first-class)
- The whole screen must mirror under `LayoutMirroring.enabled` (driven by `app.rtl` for now).
- Use Layouts + logical anchors only; NO hardcoded left/right. Avatar leads on the inline-start,
  amount/badge trail on the inline-end — these swap automatically in RTL.
- Numerals in money stay Western (already handled by CurrencyAmount).

## Data source (this phase)
Extend the existing `InvoiceListModel` (C++) if needed, but prefer doing filtering/derived
sums in a small QML-side layer or a thin C++ view-model, your choice — keep the model as the
single source. The summary counts/sums and the status/search filtering may be implemented as:
- a `QSortFilterProxyModel`-style filter in C++, OR
- a JS/QML filter over the model's roles.
Pick the simpler correct option; document which. Do NOT add editing/persistence.

## Interaction philosophy (house rules established here)
- Motion: subtle and fast (`Theme.motion.fast/base`); never block interaction. Honor
  `Theme.reduceMotion` if set.
- One primary action per screen (the indigo "New Invoice"); everything else secondary/ghost.
- Filters change results instantly (no "Apply" button).
- Selection/hover give immediate visual feedback.

## Acceptance criteria
- Builds; qmllint clean of real errors; runs alive with zero QML errors.
- All four list states reachable and visually correct.
- Search + status chips filter correctly and update the summary line.
- Mirrors correctly when RTL is toggled.
- Reuses module components (no hardcoded colors/sizes; tokens only).
- The Widgets app still builds; the existing Main remains the entry (this screen may BECOME
  the new Main content, replacing the spike list — that is desired).
