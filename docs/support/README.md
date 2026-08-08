# Occountant — Support & Operations

Internal playbooks and templates for supporting Occountant in production.

## Handling issues
- **[Support Checklist](support-checklist.md)** — the per-contact loop.
- **[Issue Triage Guide](issue-triage-guide.md)** — severity, area labels, definition of done.
- **[Incident Playbook](incident-playbook.md)** — SEV-1…4 response.

## Templates
- **[Bug Report Template](bug-report-template.md)**
- **[Feature Request Template](feature-request-template.md)**
- **[Release Notes Template](release-notes-template.md)**

## Release operations
- **[Hotfix Procedure](hotfix-procedure.md)** — out-of-cycle fixes.
- **[Rollback Procedure](rollback-procedure.md)** — fix-forward, never down.

## Foundations these rely on
- `docs/release-engineering.md` — the one-command release pipeline, signing, channels.
- `docs/release-candidate-review.md` — the C4 hardening + acceptance evidence.
- The production gate set — see the ship checklist §7.

## The one rule above all
The customer's books' integrity comes first. Protect data, never ship gate-red, and never ask a
customer for their books — the support bundle carries diagnostics only.
