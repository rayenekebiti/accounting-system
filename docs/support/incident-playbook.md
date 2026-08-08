# Incident Playbook

For the Occountant team. An *incident* is anything that threatens customer data integrity, blocks
customers from working, or exposes a security weakness. This playbook is the calm, repeatable path
through one.

## Severity levels

| Sev | Definition | Examples | Target response |
|---|---|---|---|
| **SEV-1** | Data loss/corruption risk, or security exposure | Books corrupted by an update; signing key compromise; recovery blocker firing widely | Immediate |
| **SEV-2** | Core workflow blocked for many users, no data risk | Installer fails on a common OS build; updater bricks the app on restart | Same business day |
| **SEV-3** | Meaningful bug, workaround exists | A report shows a wrong figure in an edge case | Next release cycle |
| **SEV-4** | Minor / cosmetic | Label typo; layout nit | Backlog |

## The loop

1. **Detect & acknowledge.** Log the incident (time, reporter, symptom, version/channel from the
   customer's support bundle `buildinfo.json`). Assign an owner. Set severity.
2. **Contain.** Stop the bleeding before diagnosing:
   - Bad release shipping? **Pause the update channel** so no new customers receive it.
   - Data-risk in a released build? Advise affected customers to **stop entering data and keep their
     backups** until a fix ships.
3. **Diagnose.** Reproduce from the support bundle (build info + startup diagnostics + logs +
   compatibility report). Never ask for a customer's books; the bundle carries no accounting data.
4. **Fix.** For SEV-1/2, follow the **Hotfix Procedure**. For SEV-3/4, schedule into the normal
   release.
5. **Verify.** Every fix must pass the full production gate set (see §7 of the ship checklist) plus
   a regression test that reproduces the incident. No fix ships gate-red.
6. **Release.** Ship on the appropriate channel. If a bad build reached customers, publish a
   *higher* version to roll them forward (never a lower version — the downgrade gate blocks it; see
   the Rollback Procedure).
7. **Communicate.** Tell affected customers what happened, what to do, and when it's resolved. Be
   specific and honest.
8. **Post-incident review.** Within a week: timeline, root cause, what caught it (or didn't), and
   the concrete gate/test added so it can't recur silently.

## SEV-1 specifics

- **Suspected data corruption from an update:** confirm whether the engine's own verification
  (`ACCT_COMPAT_VERIFY` / the in-app recovery check) is refusing to open, which means the safety net
  *worked* and no bad data was silently accepted. Instruct affected users to restore their most
  recent verified backup; ship a corrected build.
- **Signing key compromise:** rotate the key, bump the version, re-sign, and ship. Old signatures
  stop verifying, so tampered artifacts are rejected. Treat as a security disclosure.
- **Recovery blocker firing:** this is the product protecting the customer. Prioritise a build that
  fixes the underlying cause; in the meantime the guidance is "restore your latest backup."

## Golden rules

- The books' integrity beats every other consideration. When unsure, protect data.
- Never ship a fix that hasn't been through the gates.
- Never ask a customer to send their books. The support bundle is enough.
