# Issue Triage Guide

How to turn an incoming report into a correctly-labelled, correctly-prioritised issue.

## Step 1 — Is it reproducible / real?

- Reproduce from the support bundle. If you can't reproduce and can't infer the cause from logs,
  ask for exact steps + a fresh bundle before proceeding.
- Distinguish **bug** (software behaves wrong) from **data-entry** (the figure is what was entered)
  from **feature request** (works as designed, customer wants more). Route feature requests to the
  Feature Request Template.

## Step 2 — Severity

Use the Incident Playbook's SEV-1…4 table. The two questions that set severity:

1. **Is customer data at risk?** → SEV-1.
2. **Is the customer blocked with no workaround?** → SEV-2.

Everything else is SEV-3 (has a workaround) or SEV-4 (cosmetic).

## Step 3 — Area label

Tag the subsystem so it reaches the right person and the right gate covers the fix:

| Label | Covers | Gate |
|---|---|---|
| `installer` | install/upgrade/uninstall | `installer-test.sh` |
| `updater` | update check/stage/apply, channels | `c2test` |
| `licensing` | activation, trial, grace | `c2test` |
| `backup` | backup/restore/verify | `c2test`, `reliability.sh` |
| `recovery` | crash recovery, recovery blocker | `ptest.sh` |
| `i18n` | translations, RTL | `i18n-check.sh` |
| `a11y` | accessibility | `a11y.sh` |
| `perf` | speed, memory | `perf.sh` |
| `security` | signing, data exposure | `security-gate.sh` |
| `ui/ux` | screens, editors, wording | `itest.sh` |
| `engine` | **do not fix under launch phases** — accounting semantics are frozen | `ptest`, `compat-verify`, `fuzz` |

> An `engine` label on a launch-phase issue is a red flag: the accounting engine is frozen. If a
> report implies an engine defect, escalate — a genuine engine bug is SEV-1 and outside normal
> launch-work scope.

## Step 4 — Definition of done

Every accepted bug must ship with:

- a **regression test** that fails before the fix and passes after,
- a **green** run of the relevant gate(s) above **and** the full production set,
- proof the accounting engine is unchanged (`compat-verify` + `verifyAll` hold).

## Step 5 — Route

- SEV-1/2 → Incident Playbook + Hotfix Procedure.
- SEV-3/4 → normal release backlog, scheduled by area owner.
- Feature request → roadmap review.
