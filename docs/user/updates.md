# Update Guide

Occountant updates are safe by design: they replace the program only, never your books, and they
apply cleanly on restart.

## Checking for updates

1. Open **Settings → About**.
2. Click **Check for updates**.

Occountant tells you whether you're up to date or a new version is available. Checks are local and
never expose your data.

## Installing an update

1. If an update is available, click **Download & stage**.
2. Occountant downloads the update and verifies it's genuine and complete before accepting it.
3. **Restart Occountant.** The update is applied during startup.

If you change your mind before restarting, you can **cancel the staged update** — nothing is
changed until you restart.

## Why updates apply on restart

Occountant applies an update while the app isn't running, so an update can never collide with your
open books. If a download is interrupted, the incomplete file is discarded automatically and your
current version keeps working — you can simply check again later.

## Release channels

Most people should stay on the **Stable** channel — thoroughly tested releases.

Advanced users can opt into earlier channels to preview upcoming versions:

- **Stable** — recommended for everyone.
- **Release Candidate (RC)** — nearly-final builds.
- **Beta** — newer features, less tested.
- **Development** — the latest builds, least tested.

A channel only affects which updates you're offered. You always receive updates that are as stable
as, or more stable than, the channel you choose. Your data is never affected by your channel choice.

## Before you update

Updates don't touch your data, but making a quick backup first is always good practice (Settings →
Backup → Back Up Now).

## Signed and verified

Every update is cryptographically signed. Occountant refuses to install an update that fails
verification, so a tampered or corrupted download can never reach your computer.
