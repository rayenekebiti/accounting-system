# Support Checklist

A quick, repeatable checklist for handling an incoming support contact.

## On receipt

- [ ] Acknowledge the customer.
- [ ] Confirm a **support bundle** is attached (Settings → Diagnostics). If not, request one and
      point them to the Troubleshooting guide's "Before contacting support" steps.
- [ ] Read `buildinfo.json` in the bundle: version, build, channel, commit, Windows version.

## Classify

- [ ] Is this a **known issue**? (Check the issue tracker + release notes.) If so, share the fix/
      workaround or the version that resolves it.
- [ ] Assign a **severity** (SEV-1…4 — see the Incident Playbook).
- [ ] Is data at risk or is the customer blocked? If yes, escalate immediately per the playbook.

## Diagnose

- [ ] Reproduce from the bundle's `startup-diagnostics.txt` and `compatibility-report.txt`.
- [ ] Review `logs/*.log` for the relevant time window (money values are already redacted).
- [ ] If a crash: read the `crash/*.zip` (versions + modules only).
- [ ] Never request the customer's books — the bundle is sufficient.

## Resolve

- [ ] Provide the fix, workaround, or the guide that solves it.
- [ ] If it's a real bug, open an issue using the **Bug Report Template** and link the customer.
- [ ] For data-risk issues, remind the customer to keep their latest **verified backup**.

## Close

- [ ] Confirm the customer is unblocked.
- [ ] Record the resolution and, if it's a recurring theme, flag it for a docs or product fix.

## Common quick wins

| Symptom | First thing to try |
|---|---|
| Won't start | Restart; reinstall from the official setup; unblock the installer |
| Recovery blocker | Restore latest verified backup |
| Backup failed | Free disk space / check folder permissions |
| Restore "didn't work" | Close and reopen — restores apply on startup |
| License rejected | Re-paste key without spaces; confirm right machine |
| Update won't install | Restart the app; re-check under About |
| Wrong language | Language button, top corner |
