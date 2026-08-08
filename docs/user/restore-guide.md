# Restore Guide

Restoring replaces your current books with a chosen backup. Use it after a mistake, a move to a new
computer, or a hardware failure.

> **Restoring replaces ALL current data with the selected backup.** Anything entered since that
> backup is lost. This cannot be undone — so make sure you pick the right restore point.

## Restore on the same computer

1. Open **Settings → Backup**.
2. Find the restore point you want (check its date carefully).
3. Optionally click **Verify** first to confirm it's intact.
4. Click **Restore** and confirm.
5. **Close and reopen Occountant.** The restore is applied during the next startup.

Occountant applies restores at startup on purpose: it swaps your books safely while the app isn't
using them, so a restore can never leave you with a half-replaced set of data. If anything
interrupts the process, your original books are left untouched and the restore simply retries on
the next launch.

## Restore onto a new computer

1. Install Occountant on the new computer (**Occountant-1.0.0-Setup.exe**).
2. Copy your backup folder from the old computer (or your external drive) to the new one.
3. Open **Settings → Backup**. If your copied restore point isn't listed, place the backup folder
   in the location the Backup screen shows, then reopen the app.
4. Choose the restore point and click **Restore**, then close and reopen Occountant.
5. Re-activate your license on the new machine (**Settings → About → Activate**). See the License
   Activation Guide.

## After a crash

You usually don't need to restore after a crash. Occountant recovers automatically:

- On the next launch it repairs any half-written data and re-verifies your books.
- If the check passes, you'll see a brief confirmation and can carry on.
- If the check ever fails, Occountant **blocks** and tells you to restore your most recent backup
  rather than continue on suspect data. This is the one situation where restoring is required — and
  exactly why regular, verified backups matter.

---

See also: **Backup Guide**, **License Activation Guide**, **Troubleshooting**.
