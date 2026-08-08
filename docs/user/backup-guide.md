# Backup Guide

Your books live on your computer. A backup is your safety net against a lost or broken machine.
Occountant makes backups simple and verifiable.

## What a backup is

A backup is a complete, self-contained copy of your books saved to a folder on your computer. Each
backup is a **restore point** stamped with the date and time it was taken.

## How to back up

1. Open **Settings → Backup**.
2. Click **Back Up Now**.
3. A new restore point appears in the list with its date and size.

That's the whole process. It takes seconds.

## Verify a backup

Next to each restore point is a **Verify** button. Verifying re-checks that the backup's contents
are complete and internally consistent — proof that it will restore cleanly if you ever need it.

**Tip:** verify a fresh backup occasionally so you're never relying on a backup you've never
checked.

## How often should I back up?

- **Daily** at minimum — at the end of each working day.
- Before and after any large data entry session.
- Before installing an Occountant update (updates never touch your data, but it's good practice).

## Keep backups somewhere safe

Occountant stores restore points alongside your books on the same computer. That protects you from
mistakes and corruption, **but not from losing the whole machine** (theft, fire, drive failure).

For real protection, copy your backups to a second location:

1. Open **Settings → Backup** and note where backups are stored (the screen shows the location).
2. Copy that backup folder to an external drive or a location your own file-sync tool watches.

A backup on the same disk is convenient; a backup on a **second** disk or drive is what saves your
business.

## How many backups are kept?

Occountant keeps a rolling set of recent restore points and prunes the oldest automatically, so
your disk doesn't fill up. Copies you move elsewhere are yours to keep as long as you like.

## What backups do *not* contain

Backups contain your books and settings. They do not contain any Occountant program files — you
restore into an installed copy of Occountant (see the Restore Guide).

---

See also: **Restore Guide**, **Troubleshooting**.
