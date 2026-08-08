# Migration Guide

This guide covers moving to Occountant and moving your Occountant books between computers.

## Moving Occountant to a new computer

This is the most common migration and it's straightforward:

1. On the **old** computer, make a fresh backup: **Settings → Backup → Back Up Now**, then
   **Verify** it.
2. Copy the backup folder to the new computer (via external drive or your file-transfer method).
3. On the **new** computer, install Occountant and restore the backup — see the **Restore Guide**.
4. Activate your license on the new computer — see the **License Activation Guide**.

Your books, settings, and full history move across exactly as they were.

## Upgrading Occountant in place

Installing a newer version over an existing one is safe: your books are kept in a separate location
the installer never touches. See the **Update Guide**. There's nothing special to do — your data is
preserved automatically.

## Coming from another accounting tool

Occountant 1.0 does not yet import books from other accounting software (spreadsheets, other apps).
To start with Occountant:

1. Set your **opening balances** as of your start date by entering them as your first transactions
   (your accountant can advise on the cleanest way to record these).
2. Enter customers, suppliers, and any open invoices/expenses going forward.

Direct import from spreadsheets and other tools is on the roadmap — see Known Limitations. If you
have a large existing dataset, contact support before you begin so we can advise on the best
approach.

## Occountant version compatibility

Occountant carefully governs its own data format across versions:

- A **newer** version reads books written by an older one and upgrades them safely when needed.
- An **older** version will **refuse** to open books written by a newer one, rather than risk
  misreading them. If you see this, update to the latest version.

You never have to manage this by hand — Occountant handles it and tells you clearly if action is
needed.
