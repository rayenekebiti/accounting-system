# Pilot Operation Manual — Occountant (Phase C10)

The operator's day-to-day runbook for running a **controlled pilot** with 5–10 small businesses. It
assumes a build already cut and validated per `docs/pilot-release-guide.md`. This is the "how to run
the pilot," not "how to build it."

**Posture:** high-touch, hands-on, deliberately small. Direct-to-engineer support *is* the feedback
channel. No feature work during the pilot — only fixes for problems found in real use
(`docs/customer-feedback.md`, `docs/product-decisions.md`).

---

## 0. Before you onboard anyone

- [ ] Pilot build passes the full battery + clean-room (see release guide §6–§7).
- [ ] Customer docs ready to hand over: `docs/user/` (Quick Start, User Guide, Backup, Restore, FAQ,
      Troubleshooting, License Activation, Support).
- [ ] Feedback log started (`docs/customer-feedback.md`) and discovery log started
      (`docs/pilot-discovery-log.md`).
- [ ] Support channel live (email/phone) and the **Support ID** workflow understood (§4).
- [ ] Pilot agreement + trial terms ready (`docs/commercial-pilot.md`, `docs/pilot-agreement.md`).

---

## 1. Customer onboarding steps

Per business, on a first call/visit (~30–45 min):

1. **Confirm fit.** Record the **business type** (freelancer / consultant / small shop / repair /
   service company) in the discovery log. Set expectations: this is an early pilot; their feedback
   shapes the product; their data stays on their machine.
2. **Install together** (§2). Watch for any friction — that *is* data.
3. **First launch → onboarding wizard.** Help them set company name/address/tax number/currency/
   fiscal-year/language. Only the name is mandatory; fill the rest so their first invoice is complete.
4. **Do one real transaction each, live:**
   - Create a real **customer**.
   - Issue a real **invoice** → **Export PDF** → confirm it looks right to send.
   - Record the **payment** when it arrives → allocate it.
   - Record one **expense**.
   - Open **Ledger → Trust** and show them the books are balanced and backed up.
5. **Show them Backup** (§3) — make one together, show where it lives, explain restore-needs-restart.
6. **Give them their Support ID** (§4) and the support contact. Tell them exactly how to report a
   problem (§7).
7. **Log the baseline** in the discovery log: install success, time-to-first-invoice,
   time-to-first-payment.

Leave them able to run their *own* daily cycle unaided. The pilot's whole question is whether they
can — so resist doing it *for* them beyond the first walkthrough.

---

## 2. Installation checklist (per machine)

- [ ] Windows 10 or 11, 64-bit.
- [ ] Run the **signed** `Occountant-<ver>-Setup.exe`. If SmartScreen warns (until the production
      cert is in place), confirm publisher and proceed — note this as a known pre-GA rough edge.
- [ ] Complete install → launch → **onboarding wizard appears**.
- [ ] Switch language once to confirm the user's language (EN/FR/AR) renders; Arabic shows RTL.
- [ ] Create one throwaway customer to confirm save works, then delete or keep as a real one.
- [ ] Confirm the **data location** (under the user profile) and that it's on a backed-up drive if the
      business has one.
- [ ] Record **install success = yes/no** (+ any error) in the discovery log.

If install fails: capture the error, and if the app launched at all, a **Support Bundle** (§4). A
`STATUS_DLL_NOT_FOUND` means a packaging miss — stop and re-verify the release guide §2–§3 before
continuing with other customers.

---

## 3. Backup procedure

Teach and confirm this on day one — it's the foundation of trust.

- **Automatic:** Occountant makes hourly backups while running (retention keeps recent copies).
- **Manual:** **Settings → Backup → Back Up Now**. A backup is a full, self-contained copy of the
  books (including the authoritative `audit.log`).
- **Verify:** **Settings → Backup → Verify** on any backup confirms its history is intact.
- **Where:** backups live under the data directory's `backups/`. If the business has external/cloud
  storage, have them copy that folder there periodically (Occountant does not upload anything).

**Operator guidance to give the customer:** "Back up before anything unusual (big month-end, a
Windows update). Verifying takes seconds. A verified backup is your safety net."

---

## 4. Support procedure

1. **Every pilot user has a Support ID** — a short, non-PII token (e.g. `OCC-3F9A-2B71`) shown in
   **Settings → Diagnostics → Support**. Ask them to quote it in every report so their reports line
   up. It identifies nothing about them or their machine; it's a random label.
2. **For any bug/problem, ask for a diagnostics bundle:** **Settings → Diagnostics → Create support
   bundle**. It writes `SupportBundle.zip` under their data dir's `support/` folder and shows the
   path. It contains app health only — **build info, logs (money redacted), crash reports, startup
   diagnostics, compatibility report — never accounting data** (allowlist-enforced; proven by
   `security-gate.sh`). Nothing is transmitted automatically — the user chooses to send the file.
3. **Correlate** the received bundle to the user via the Support ID printed in `bundle-manifest.txt`.
4. **Triage** with the A–E classification (`docs/customer-feedback.md`). Correctness / data-safety /
   crash → immediate (§ priority order there).
5. **Close the loop** — tell the user what happened; a fixed issue cites the gate/commit now covering
   it. Ship fixes only through the full battery.

Response targets (pilot hypothesis; set with the commercial owner): acknowledge within one business
day; a correctness/data-safety/crash report gets a same-day plan.

---

## 5. Data recovery procedure

Occountant is crash-safe by design; recovery is rarely needed and never destructive.

**Case A — app crashed / power loss.** On next launch it **replays the journal automatically** and
opens the recovered books. If verification fails, a **Recovery Blocker** stops it from running on
suspect data (by design — never operate on unverified books). If you see the blocker: do **not**
force it; take a Support Bundle and restore from a **verified** backup (Case B).

**Case B — restore from a backup.**
1. **Settings → Backup**, pick the backup, **Verify** it first.
2. **Restore** it (confirm the warning — this replaces current data). A **corrupt or history-less
   backup is refused** and your current data is left untouched.
3. **Restart** Occountant to complete the restore (files are locked while running; the banner says
   so). The swap is atomic (temp→rename) — an interrupted restore can't half-replace the books.

**Case C — machine lost / reinstalling.** Uninstall **keeps** the data dir. Reinstall over it and the
books reopen. If moving to a new machine, copy the data directory across, then install and launch.

**Golden rule:** never hand-edit files in the data directory. Recovery is always: verify a backup →
restore → restart. If in doubt, take a Support Bundle and involve the operator before acting.

---

## 6. Update procedure (pilot posture)

For a controlled pilot, updates are **operator-delivered**, not auto-fetched from the internet.

1. The operator builds + signs a new version (`docs/pilot-release-guide.md`); update payloads are
   signed with **Ed25519** and the app verifies them with its embedded public key
   (`docs/update-signing.md`).
2. Deliver the signed installer to pilot users directly (the app's update source is a local path in
   v1 — public HTTPS hosting is a GA item, not a pilot one).
3. The user runs the new installer **over the top**. Data is preserved; a **downgrade is refused**.
4. Confirm post-update: app launches, trial balance still 0, their data intact. Note the version in
   the discovery log.

Do **not** advertise auto-update as a live/secure channel to pilot users until production key +
HTTPS hosting exist. For the pilot, "we'll send you updates" is the honest description.

---

## 7. Bug reporting process (what to tell the customer)

Give every pilot user this one-paragraph instruction:

> *If something looks wrong or confusing: (1) note what you were doing and what you expected; (2) open
> **Settings → Diagnostics** and copy your **Support ID**; (3) if it's a bug, click **Create support
> bundle** and attach the `SupportBundle.zip` it saves; (4) email it all to `[support address]`.
> The bundle never contains your customers, invoices, or numbers — only technical health info.*

Operator side, for each report, record in the feedback log: Support ID, business type, signal type
(install / first-use / workflow / missing / bug / perf / trust), verbatim description, bundle
reference, class (A–E), severity, status, and resolution+evidence.

**Escalate immediately** (stop-the-line) if a report indicates: a **wrong accounting number**, a
**data-loss/corruption** risk, or a **crash on normal use**. Everything else batches into weekly
triage.

---

## 8. End-of-pilot handoff

At the end of the window, roll the discovery + feedback logs into `docs/phase-c10-report.md`:
real users, industries, problems found/fixed, features requested/rejected, and conversion
willingness — with an honest verdict. Do not widen the pilot or take public payment until that report
shows correctness held, no data loss occurred, users worked unaided, and at least one customer is
willing to pay.
