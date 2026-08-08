# Hostile First-Time-User Audit — Occountant (Phase C11)

**Method:** I acted as a skeptical operator and went through the **actual shipping QML/VMs** (not the
acceptance-test narrative) as each persona would on a fresh install — reading the real navigation,
the real first-invoice flow, the real error-recovery paths — and looked for where a non-technical
owner gets **blocked or confused**. Fixes were applied **only** where a core workflow is blocked.

**One fix was made** (customer dead-end, below). Everything else is recorded as a finding for the
pilot to confirm with real users — not "fixed" speculatively.

---

## Cross-persona findings (from the real UI)

### ⛔→✅ FIXED — First invoice dead-ends with no customers
- **Evidence:** fresh install lands on **Invoices** (empty). Its empty-state CTA is **"New Invoice."**
  Tapping it opens the editor with a **required Customer dropdown** bound to `customerOptions`, which
  is **empty**, and the editor had **no way to add a customer**. The product's own primary
  call-to-action led straight into a wall.
- **Why it's a blocker (not mere friction):** the *guided* first action of the whole app dead-ends
  with no signpost — exactly the moment a non-technical owner gives up.
- **Fix (UI-only, no engine):** when there are zero customers, the invoice editor now shows
  *"You don't have any customers yet — add one to invoice them."* with an **Add customer** button that
  opens the customer editor over the sheet (Popups stack); on save, the existing
  `refreshCustomerOptions()` wiring repopulates the picker and the draft is preserved. Localized
  EN/FR/AR; `itest` green (no QML errors).

### ⚠ Finding — "Reports"/tax live under **"Ledger"** (accounting jargon)
- Trial Balance, Tax/VAT summary, P&L (via export), Trust, and Period close are all under the
  **Ledger** nav item. A retail owner or freelancer looking for "my reports" or "my VAT" will not
  obviously associate them with the word *Ledger*. **Not a blocker** (they're reachable), but a likely
  confusion point. *Recommendation to test:* rename/alias the nav item ("Reports & Ledger") or add a
  "Reports" entry point — **only if** pilot users actually miss it. No change made without evidence.

### ⚠ Finding — Status dropdown lets a user set **"Paid"/"Overdue"** by hand
- The invoice **Status** select offers Draft/Posted/Paid/Overdue/Void as manual choices. Marking an
  invoice **"Paid"** directly — instead of recording a payment — would make the invoice *look* settled
  while **AR and the payment record disagree**. The engine's postings remain correct, but the UI
  *invites* a data-hygiene mistake and a confused "why doesn't my outstanding match?". **Not changed**
  (it borders on posting semantics, which this phase must not touch), logged as a design risk to watch
  and to cover in onboarding ("record a payment; don't hand-set Paid").

### ⚠ Finding — No dashboard / "getting started" home
- The app opens on the Invoices list; there is no overview or first-run checklist. Fine for an
  invoicing-first tool, but a new owner gets no "here's what to do next." *Watch, don't build* unless
  users ask.

### ✅ Verified good (claims that held up under inspection)
- `beginNew()` **auto-fills** the next invoice number, today's issue date, a +30-day due date, and a
  blank line — the user invents nothing. Genuinely low-friction.
- Line editing is keyboard-driven (Return advances cells, adds a line at the end); totals recompute
  live in int64 cents.
- Recording a payment **auto-opens allocation** — the two-step is guided, not hidden.
- Expenses have an explicit **Void** (open period) with a compensating entry; closed periods force a
  reversal. Recovery paths exist.

---

## Persona verdicts

### Persona A — Small retail shop owner
- **Navigation:** understandable (icons help); **"Ledger"** for reports is the one snag.
- **First invoice / payment / fix / documents:** all completable (dead-end fixed). Void-via-status is
  non-obvious for "cancel this sale."
- **Where they'd get confused:** finding VAT/reports under "Ledger"; high daily volume has no
  bulk/fast entry (not a pilot blocker, a scale question).
- **Verdict:** usable for a low-to-moderate volume shop; **weak fit** for high-count cash retail.

### Persona B — Freelancer
- **Best fit.** Few customers, high-value VAT invoices, clean PDF to send, clear "who owes me,"
  VAT summary for the return. The add-customer dead-end was their main first-run stumble — now fixed.
- **Where they'd get confused:** minimal. Reports under "Ledger"; manual "Paid" status temptation.
- **Verdict:** **strongest fit.** This is the persona to recruit first.

### Persona C — Small service company (consultant)
- **Fit, with one gap.** VAT invoices, retainers, monthly reporting, and (new in C9) an on-screen
  **period close** all work. **Recurring/retainer invoices are not automated** — a monthly retainer is
  re-entered by hand each month (candidate PD-04, not built without evidence).
- **Verdict:** **good fit** for low invoice counts; re-keying retainers is the friction to watch.

### Persona D — Accountant managing several clients
- **⛔ Structural mismatch.** Occountant is **single-company per install**: one company profile
  (`SettingsViewModel`/onboarding), one data directory, **no in-app company switching**. An accountant
  with several clients would need a separate install/data dir per client and could not switch between
  client books inside the app.
- **Not fixable within this phase** (multi-company is an architecture change, explicitly out of
  scope — and would touch storage/governance). This is an **ICP finding, not a bug**: the product
  today serves **the business owner keeping their own books**, *not* the multi-client
  bookkeeper/accountant.
- **Verdict:** **out of the current ICP.** Do not recruit accountants-with-many-clients as pilot
  users expecting to manage all clients in Occountant; at most, recruit an accountant as an *advisor*
  to a single owner's books.

---

## What this audit changed vs. only recorded

| Finding | Severity | Action |
|---|---|---|
| First-invoice customer dead-end | Blocker | **Fixed** (add-customer affordance) |
| Reports/tax under "Ledger" jargon | Usability | Recorded — change only on pilot evidence |
| Manual "Paid"/"Overdue" status | Data-hygiene risk | Recorded — covered in onboarding; no engine change |
| No dashboard/home | Minor | Recorded — watch |
| Void-via-status non-obvious | Usability | Recorded — covered in onboarding |
| Single-company (Persona D) | Structural | Recorded — **narrows the ICP**, not fixed |

**Discipline note:** only the true blocker was fixed. The rest are logged for the real pilot to
prove or disprove — no speculative UI churn, no engine changes.
