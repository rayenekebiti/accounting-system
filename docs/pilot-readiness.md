# Pilot-Customer Readiness — Occountant (Phase C7)

**Goal:** make Occountant safe for real SMB pilot users — *not* feature expansion. No ERP/POS/cloud.
Audit the five readiness areas; implement only what blocks a real pilot; regression-gate each fix.

**Method:** every fix is driven through the real ViewModels/services and proven by a headless gate.
New gate: `ACCT_PILOT=safety|export|all` (backup/restore + export). Full battery re-run after changes.

**Battery (all green):** `ACCT_PILOT=all` 24/0 · `ACCT_HOSTILE=all` 0 findings · `ACCT_ITEST` 117/0
(seeded) · `ACCT_PTEST=all` 268/0 · `ACCT_FUZZ=all` 13/0 · `ACCT_COMPAT_VERIFY` replay-equivalence
held · acceptance (cafe/retail/clinic/repair) 25/0 each.

---

## 1. Data safety

### 🔴 D1 — Restore overwrites live books with an UNVERIFIED backup (Critical) — FIXED
- **Customer impact:** Restoring a corrupt or history-less backup destroyed the current books (the
  staged set replaces the live files on next start) **and** then failed to open — total, unrecoverable
  data loss during the one operation a user runs precisely to *avoid* losing data.
- **Root cause:** `BackupViewModel::restore()` staged the chosen backup with no integrity check;
  `main_quick` swapped it over the live files before the store opened.
- **Minimal fix:** `restore()` now opens the backup's `audit.log` (validates CRC + gap-free history)
  and refuses — leaving live data untouched — if it is missing or corrupt (`quick/BackupViewModel.cpp`).
- **Regression:** `ACCT_PILOT=safety` — corrupt backup refused, history-less backup refused, verified
  backup accepted, live data intact after refusals.

### 🟠 D2 — Startup restore swap could lose live files on a mid-swap I/O failure (High) — FIXED
- **Customer impact:** A disk-full/error partway through applying a restore left the data dir with some
  live files already deleted and their replacements not yet written.
- **Root cause:** the apply-restore loop did `remove(target)` then `copy(staged→target)`.
- **Minimal fix:** copy each staged file to `target.restoring`, then **atomically rename** over the
  target — the live file is only replaced once its full replacement exists (`quick/main_quick.cpp`).
- **Regression:** covered indirectly by `ACCT_PILOT=safety` (verified-backup staging) + `ACCT_ITEST`.

### ✅ Verified sound (no change needed)
- **Backup capture:** the active path (`BackupScheduler`, used by `main_quick`) copies **all** data
  files incl. `audit.log`/`audit.cursor` and `verify()`s the log — the authoritative history is
  captured, not just `.dat` projections.
- **Migration:** governance/compat refuse-newer + forward-migration adoption work; opening newer books
  is refused loudly rather than silently reinterpreted.

### 🟡 D3 — Notes (not blocking)
- Dead `storage/BackupService` copies only `*.dat` (would miss `audit.log`); unused by the Quick app —
  latent trap, recommend deleting. The v1 semantic-migration registry is empty, so the *first* update
  that changes a governance axis needs a registered path before shipping (not a single-version pilot
  concern).

---

## 2. First-run experience

### ✅ Works for pilot
- Empty books open cleanly; chart of accounts + default tax codes auto-bootstrap; every screen has an
  empty state; company identity persists (Settings → QSettings) and now flows onto exported invoices.

### 🟡 F1 — No onboarding wizard; company identity is blank by default (Medium — should-fix, not blocking)
- **Customer impact:** A new user isn't guided to set company name/address/tax id, so early exported
  invoices carry an empty company header until they visit Settings.
- **Minimal fix (recommended, not implemented — borders on feature work):** prompt for company name on
  first run, or show a one-line "Set your company details" banner until set.
- **Regression (when done):** assert exported invoice header is non-empty after first-run company set.

---

## 3. Daily accounting workflow

### 🟠 W1 — No way to export/share invoices or reports (High) — FIXED
- **Customer impact:** An invoicing/accounting product a business could not get information *out* of —
  no invoice to send a customer, no figures to hand an accountant/tax authority. A hard pilot blocker.
- **Root cause:** no export/print/CSV path existed anywhere.
- **Minimal fix:** `ExportService` (headless, reads the authoritative engine) + `ExportViewModel`
  writing portable **CSV** to `<dataDir>/exports/`, wired as `exportVm`, with **Export CSV** buttons on
  the invoice editor (an existing invoice), Trial Balance, and Tax Summary screens. Covers: invoice
  (company header + lines + totals + balance due), trial balance, income statement (P&L), tax/VAT
  summary. No new accounting behaviour — every number is read from `AuditJournal`.
- **Regression:** `ACCT_PILOT=export` — invoice/trial-balance/P&L/tax CSVs produced with correct totals
  ($100 net + 20% VAT = $120, trial balance totals 0, $20 output VAT, company identity present).

### ✅ Ledger-correct (fixed in Phase C6, re-verified here)
- Create invoice → posts Dr AR / Cr Revenue / Cr Tax. Receive payment → posts Dr Cash / Cr AR
  (`ACCT_ITEST` now asserts the receipt debits Cash). Record expense → posts. View reports → Trial
  Balance + Tax Summary screens, all reconciled (`ACCT_HOSTILE=all` 0 findings).

### 🟡 W2 — Period close has no on-screen form yet (Medium — not a 30-day blocker)
- **Customer impact:** The engine + `PeriodCloseViewModel` (wired as `periodVm`) freeze periods and
  refuse in-period edits, but there is no QML form to enter the range. Period close is a month/quarter-
  end task; a 30-day pilot reaches its first close at/after day 30 (often done with the accountant).
- **Minimal fix (recommended):** a small "Close period" form (label + start/end) bound to `periodVm`.
- **Regression (exists at the VM level):** `ACCT_HOSTILE=period` proves a closed period rejects invoice
  edits + expense voids.

---

## 4. Customer support readiness — ✅ Ready

- **Diagnostics bundle:** `SupportBundle` composes build info + logs + crash reports + startup
  diagnostics + compatibility report into one zip **without accounting data** (Diagnostics screen +
  `ACCT_SUPPORT_BUNDLE`). Deterministic, offline.
- **Error messages:** editors emit human-readable `saveFailed`/`actionFailed`; storage exposes
  `lastInitError`; restore now gives explicit "your current data is untouched" refusals.
- **Recovery:** crash recovery replays journals on open; `RecoveryDialog`/`RecoveryBlocker` surface it.
- **Logs:** `prodlog` production logging with rotation. No blocker found.

---

## 5. Commercial blockers

- **Licensing:** a usable trial is issued on first run (`LicenseManager`, acceptance-verified). ✅
- **Update delivery:** `UpdateManager` checks/verifies/stages/applies + rolls back. Functionally ready.
  ⚠️ **Known security item (out of scope here, fix before GA):** updates are signed with a symmetric key
  embedded in the binary — forgeable. Not a functional 30-day blocker; do not advertise auto-update as
  a security boundary until it is asymmetric.
- **Installer:** Inno-based installer exists. ⚠️ **Packaging risk to validate before the pilot build
  ships:** the MinGW/Qt DLL closure must be clean-room verified on a fresh machine (per
  `deployment-packaging`) or the app fails with `STATUS_DLL_NOT_FOUND`. Verify on a clean VM before
  handing the installer to a pilot customer.

---

## Files changed (this phase)
- `quick/BackupViewModel.cpp` — verify backup before staging a restore (D1)
- `quick/main_quick.cpp` — atomic restore swap; wire `exportVm` (D2, W1)
- `quick/ExportService.{h,cpp}`, `quick/ExportViewModel.{h,cpp}` — CSV export (W1)
- `quick/InvoiceEditorViewModel.h` — expose `editId` for invoice export (W1)
- `quick/qml/{InvoiceEditor,TrialBalanceScreen,TaxSummaryScreen}.qml` — Export CSV buttons (W1)
- `quick/pilot_checks.{h,cpp}` + `CMakeLists.txt` + `main_quick.cpp` — `ACCT_PILOT` regression gate
- `quick/itest.cpp` — payment assertion updated to the corrected 2-event (settlement + posting) behaviour

---

## Verdict — Can a real small business use Occountant for 30 days without developer intervention?

### ✅ Yes — for the core daily accounting workflow, with two setup conditions on the vendor.

The pilot-blocking issues are fixed and regression-gated: books are **safe** (backups capture and verify
the authoritative history; restore now refuses corrupt/history-less backups instead of destroying data),
the daily workflow is **ledger-correct** (invoice → payment → expense → reports all reconcile), and a
business can now **deliver** invoices and reports as CSV. Support tooling (diagnostics bundle, readable
errors, crash recovery, logs) is in place, and licensing issues a working trial.

**Two conditions the vendor must meet before handing over the pilot build (not code blockers):**
1. Ship the installer only after a **clean-room DLL-closure verification** on a fresh machine (§5).
2. Have the user set **company identity** at setup (§F1) so exported invoices are complete.

**Should-fix during the pilot (not 30-day blockers):** a first-run onboarding prompt (F1), an on-screen
period-close form (W2), and hardening update signing to asymmetric before any wide release (§5).

Net: a controlled 30-day pilot can run without developer intervention for day-to-day accounting. The
remaining items are UX/packaging polish and a pre-GA security hardening — none of which stop a small
business from invoicing, taking payments, recording expenses, reporting, exporting, and safely backing
up for 30 days.
