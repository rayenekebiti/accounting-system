# Support System — Architecture & Privacy (C12)

Technical documentation for the Early Access / Support Center infrastructure. It records the
architecture, the strict data-privacy rules, and the future roadmap. Everything here lives **above**
the accounting engine and is subject to the hard constraint that customer financial data never leaves
the machine unless the user explicitly exports it.

---

## Architecture

All components sit in the app/UI layers; **none** touch EventLog, storage format, posting, replay,
governance, compatibility, or financial semantics.

### Data model (local, above the engine)
- **`app/SupportTicket.h`** — a support ticket value type: `id`, `category`, `severity`,
  `whatHappened` / `expected` / `steps`, `createdIso`, `status`, `valuable`, `rewardNote`,
  `supportId`, `bundlePath`. JSON (de)serialization. Statuses: **Received → Reviewing → Confirmed →
  Fixed → Released**.
- **`app/SupportTicketStore.{h,cpp}`** — a tiny file-backed store persisting a JSON array to
  `<dataDir>/support/tickets.json`. Atomic writes (temp→rename). Single-user, desktop scale. Creating
  or updating a ticket authors **no accounting events**.
- **`app/SupportId.{h,cpp}`** *(existing, C10)* — stable, random, non-PII install identifier
  `OCC-XXXX-XXXX`, persisted in QSettings. Not hardware-derived. Included in bundles for correlation.
- **`app/SupportBundle.{h,cpp}`** *(existing, C10)* — assembles the privacy-safe diagnostics zip by
  **allowlist** (see rules below). Now also records the Support ID in its manifest.

### View models (Qt, above the engine)
- **`quick/EarlyAccessViewModel.{h,cpp}`** — the welcome-notice state machine over QSettings
  (`earlyAccess/acknowledgedMajor`, `earlyAccess/suppressed`). `shouldShow` is true on first launch or
  after a **major** version bump; `acknowledge()` / `remindLater()` / `dontShowAgain()`. No events.
- **`quick/SupportCenterViewModel.{h,cpp}`** — files reports (creating local tickets), attaches
  privacy-safe bundles, exposes the Support ID + ticket list, and records reward eligibility. Never
  mutates the engine.

### UI (QML)
- **`EarlyAccessDialog.qml`** — the welcome notice (Continue / Remind me later / Don't show again),
  shown at startup (when onboarding isn't) and reachable from **About → Early Access Program**.
- **`SupportCenter.qml`** — **Settings → Support Center**: Support ID + copy + "Create diagnostics
  bundle"; a "Report a problem" form (category, severity, what/expected/steps, attach-diagnostics
  toggle); and a local list of your reports with status.
- Wiring: a new **Support** tab in `SettingsWorkspace.qml`; an **Early Access Program** card in
  `AboutScreen.qml` (signal bubbles to `Main.qml`, which opens the dialog).

### Data flow (report a problem)
```
User → SupportCenter.qml → SupportCenterViewModel.submitReport(...)
     → SupportTicketStore.add()  →  <dataDir>/support/tickets.json   (local metadata only)
     → (optional) SupportBundle.generate() → <dataDir>/support/SupportBundle.zip (app health only)
Nothing is transmitted. The user chooses whether to send the file.
```

---

## Data privacy rules (enforced, not aspirational)

1. **Financial data never leaves the machine automatically.** No network calls, no telemetry, no
   sync. The only egress is a file the user *manually* exports/sends.
2. **The diagnostics bundle is allowlist-only.** It may contain **only**: Occountant version, OS
   information, language, build channel, engine version, verification/compatibility status, Support
   ID, and **redacted** logs (currency values masked at write time). It is assembled by matching an
   allowlist of file suffixes, so even a misconfigured path cannot pull in books.
   - **Forbidden and provably excluded:** customers, suppliers, invoices, payments, expenses, company
     identity, and any accounting record (`*.dat`, `audit.log`, journals, `compat.manifest`).
3. **Tickets carry only what the user typed** plus category/severity/status and the anonymous Support
   ID. No accounting records are read into a ticket.
4. **The Support ID is anonymous.** Random (v4-UUID-derived), not hardware fingerprinting, not
   personally identifiable, stored locally.
5. **Support flows author no accounting events** and never change replay-equivalence — verified by
   `ACCT_EARLY_ACCESS` (no-events + replay-equivalence-unchanged + leak-canary assertions) and by
   `security-gate.sh` / `ACCT_C2TEST` (no data-file name or bytes in any artifact).

### Verification (`ACCT_EARLY_ACCESS`, 24 checks)
- Notice shows once and never repeats after acknowledgement; returns on a major update; remind/suppress
  behave correctly and persist.
- Support ID is well-formed and stable across reads.
- Filing a ticket, exporting diagnostics, and recording reward eligibility author **no** accounting
  events; **replay-equivalence and trial-balance remain intact**.
- The bundle is a valid zip that **excludes** a seeded customer "leak canary" and all accounting
  data-file names, and **includes** the Support ID.
- The Arabic catalog loads and an Early Access string is localized (EN/FR/AR are fully translated;
  `i18n-check` enforces coverage).

---

## Future roadmap (explicitly deferred — build only with evidence)

The data model is intentionally shaped so these *could* be added later **without** compromising
privacy, but none are built now:

- **Local ticket management UI polish** — operator-side status editing is currently file-level; a
  richer local view could come if support volume warrants it.
- **Manual, user-initiated send** — a one-click "email this bundle" helper (still user-initiated, no
  background transmission). Only if users ask.
- **Optional, opt-in, aggregated feedback export** — a user-exported summary they *choose* to share.
  Would require explicit consent and a `product-decisions.md` entry; **never** silent.
- **A support back-office** — a cloud dashboard for triaging tickets is **out of scope** and would
  live entirely on the vendor side, fed only by bundles users choose to send. **Not** built here.

### Non-goals (hard line)
No cloud accounting, no data synchronization, no telemetry, no automatic transmission of anything, and
no change to the accounting engine. The support system exists to help users and collect **chosen**
feedback while keeping the privacy-first, offline-first architecture intact.
