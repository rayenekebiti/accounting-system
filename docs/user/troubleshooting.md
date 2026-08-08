# Troubleshooting

Most issues are quick to resolve. Find your symptom below. If nothing here helps, generate a
support bundle (**Settings → Diagnostics**) and contact support.

## Occountant won't start

- Restart your computer and try again.
- Make sure you installed with the official **Occountant-1.0.0-Setup.exe** and didn't move the
  program files by hand.
- If Windows blocked the installer, right-click it → **Properties** → **Unblock**, then reinstall.

## "Your data was verified" message on startup

This is good news, not an error. It means Occountant recovered cleanly from an unexpected shutdown
and confirmed your books are consistent. You can continue normally.

## Occountant blocks with a recovery screen

This means the automatic check found an inconsistency and Occountant is protecting you by refusing
to continue on suspect data. **Restore your most recent verified backup** (see the Restore Guide).
This is rare and is exactly why regular, verified backups matter.

## A backup failed

The message will say why — usually the folder isn't writable or the disk is full. Free up disk
space or check the backup location's permissions, then try **Back Up Now** again.

## A restore didn't seem to apply

Restores are applied when Occountant next starts. **Close and reopen** the app. If the app was
interrupted during the swap, your original books are safe and the restore retries on the next
launch.

## My invoice/payment totals look wrong

- Check the invoice lines: quantity × unit price, plus the VAT % per line.
- For payments, confirm how much you **allocated** to each invoice under Payments. Unallocated
  money sits as a credit until you apply it.
- Open **Ledger → Trial Balance** — if it shows *Balanced*, the books are internally consistent and
  the figure you're questioning is almost certainly a data-entry value, not a bug.

## The app is in the wrong language

Use the language button in the top corner to switch between English, French, and Arabic. This only
changes the display — your data is never affected.

## Text or layout looks cramped at large system font sizes

Occountant scales with your Windows display settings. If layout looks tight, try Windows **Settings
→ Display → Scale** at 100–150%. Report extreme cases to support with a screenshot.

## The license won't activate

- Copy the key exactly, with no extra spaces.
- Confirm you're activating on the computer you intend to use.
- See the License Activation Guide, then contact support with your support bundle if it still
  fails.

## An update won't install

Updates apply when you restart Occountant. Close and reopen the app. If an update was interrupted,
Occountant discards the incomplete download safely and you can check for updates again under
**Settings → About**.

---

## Before contacting support

1. Note what you were doing when the problem happened.
2. Generate a **support bundle**: Settings → Diagnostics → (create support bundle).
3. Send the bundle with a short description. It contains diagnostics and logs only — never your
   customers, invoices, or amounts.
