# Rollback Procedure

"Rollback" for a desktop app is different from a server. There is no button that un-ships a release
already on customers' machines. This document defines what we actually do.

## Principle: roll *forward*, never *down*

Occountant intentionally **refuses to install an older version over a newer one** (the installer's
downgrade gate) and **refuses to open books written by a newer version with an older one** (the
engine's compatibility governance). This protects customers' data — an older build must never
reinterpret newer books. Therefore we never "roll back" a version number; we fix forward.

## If a bad release reached customers

1. **Stop the spread.** Pause the affected update channel so no new customers are offered the bad
   build. Existing installs already have it — the fix is a new, higher version.
2. **Assess data impact.**
   - If the engine's verification/recovery **refused to open** suspect data, the safety net worked
     — no bad data was silently accepted. Guide customers to restore their latest verified backup.
   - If the bad build only affected a non-engine surface (UI, updater, installer), customer data is
     intact; the fix-forward build resolves it on restart.
3. **Fix forward.** Follow the **Hotfix Procedure**: patch, bump to a higher version, pass all
   gates, ship. The updater rolls customers up to the corrected version.
4. **Un-publish the bad artifact** from the download page and update source so no fresh installs or
   updates pull it. Keep it archived internally for the post-incident review.

## Customer-side recovery (data)

The customer's own "rollback" is **restore from backup** — replacing current books with a known-good
restore point. This is why the Backup and Restore guides, and the habit of *verified* daily
backups, are load-bearing for the whole product. Point customers there when a bad build has affected
their data entry.

## Pre-release safeguards that make rollback rare

- Stage risky releases on **RC/Beta** channels first; Stable customers only receive vetted builds.
- The full production gate set must be green before any release.
- `installer-test.sh` proves upgrade preserves data and downgrade is refused; `reliability.sh`
  proves repeated upgrades don't lose data.

## What we never do

- Never publish a **lower** version to "undo" a release (blocked by design, and it would strand
  newer books).
- Never ask customers to hand-edit their data files.
- Never ship an un-gated emergency build.
