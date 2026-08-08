# Admin Checklist — Occountant Pilot (C11)

Operator-side checklist for each pilot business. Complements the build/release checklist
(`docs/pilot-release-guide.md`) and the operations runbook (`docs/pilot-operation-manual.md`). Print
one per customer.

```
Pilot business #____   Type: (freelancer | consultant | small shop | repair | service)
Support ID: OCC-____-____   Operator: __________   Date: __________
```

## Before the customer (per machine)

### Installation verification
- [ ] Windows 10/11 64-bit.
- [ ] Signed `Occountant-<ver>-Setup.exe` installs cleanly (note any SmartScreen prompt).
- [ ] App launches → **onboarding wizard appears** (proves first-run path).
- [ ] Switch language once to the customer's language; **Arabic → RTL** renders (proves catalogs).
- [ ] Create + delete a throwaway customer (proves save/persist).
- [ ] No `STATUS_DLL_NOT_FOUND` — if seen, STOP; re-verify DLL closure (release guide §2–§3).

### Backup verification
- [ ] **Settings → Backup → Back Up Now** creates a restore point.
- [ ] **Verify** the backup passes.
- [ ] Confirm the data + backups location under the user profile; note it below.
- [ ] (If corrupt-restore refusal needs demonstrating, it's covered by `ACCT_PILOT=safety` — don't
      corrupt a real customer's backup to prove it.)

### License activation
- [ ] Trial issued on first run (full-feature). Note trial length/expiry.
- [ ] If a purchased key is being used: **Settings → About → activate**; edition unlocks; data
      unchanged (`docs/user/license-activation.md`).

### First company setup
- [ ] Onboarding wizard completed: **business name** (required) + address + tax id + currency +
      fiscal-year + language.
- [ ] Confirm the company header shows on a test **Export PDF** invoice.

```
Data location: __________________________     Trial expiry: __________
```

## Hand-over
- [ ] Walk the owner through `customer-onboarding.md` Steps 2–8 (first invoice + first backup, live).
- [ ] Give them: their **Support ID**, the support contact, `docs/user/` (Quick Start, FAQ, Backup,
      Restore), and the pilot agreement (`docs/pilot-agreement.md`).
- [ ] Explain: reports are under **Ledger**; record payments (don't hand-set "Paid"); restore needs a
      restart; uninstall keeps data.
- [ ] Start the discovery record (`docs/pilot-discovery-log.md`): install success, time-to-first
      invoice, time-to-first payment, business type.

## Ongoing (weekly)
- [ ] Check in; log any issue in `docs/customer-feedback.md` (class A–E) + update the discovery log.
- [ ] Escalate immediately any **wrong number / data-loss / crash** report (stop-the-line).
- [ ] Ship fixes only through the full battery (release guide §6).

## Known rough edges to pre-empt (set expectations honestly)
- [ ] Unsigned/SmartScreen warning until the production code-signing cert is in place.
- [ ] Updates are **delivered by the operator**, not auto-fetched, during the pilot.
- [ ] **Single company per install** — not for managing multiple clients' books (see ICP).
- [ ] Reports export **CSV** (invoices get PDF); retainers are re-entered manually.
