# Hotfix Procedure

A hotfix is an out-of-cycle release that fixes a SEV-1 or SEV-2 issue. Speed matters, but the gates
are non-negotiable — a hotfix that skips verification can turn a SEV-2 into a SEV-1.

## Preconditions

- The issue is reproduced and its root cause is understood.
- The fix is **scoped**: smallest possible change, no accounting-engine edits (the engine is
  frozen), no opportunistic refactors.

## Steps

1. **Branch** from the released version's commit (not from `main` tip) so the hotfix carries only
   the fix, not unrelated in-flight work.
2. **Write the regression test first.** It must fail on the released build and pass with the fix.
   Add it to the relevant gate (see the Issue Triage area→gate table).
3. **Apply the minimal fix.**
4. **Bump the patch version** (e.g. 1.0.0 → 1.0.1). Never reuse or lower a version number — the
   downgrade gate blocks lower versions and the updater rolls customers *forward*.
5. **Run the full production gate set** — everything must be green:
   ```
   bash tools/ptest.sh        bash tools/itest.sh       bash tools/fuzz.sh
   bash tools/perf.sh         bash tools/security-gate.sh  bash tools/i18n-check.sh
   bash tools/installer-test.sh  bash tools/acceptance.sh   bash tools/a11y.sh
   # + ACCT_C2TEST and ACCT_COMPAT_VERIFY
   ```
   Confirm the accounting engine is unchanged: `ACCT_COMPAT_VERIFY` and `verifyAll` hold.
6. **Build the release** with `tools/release.sh --version <patch> --channel stable` (signs +
   produces installer/portable/manifests/checksums; degrades gracefully where signing tools are
   absent — see `docs/release-engineering.md`).
7. **Stage on a faster channel first** (RC/Beta) if the fix is risky and time allows; otherwise ship
   Stable directly for a SEV-1.
8. **Publish** the manifest + artifacts. Customers are offered the higher version and roll forward
   on restart.
9. **Write release notes** (Release Notes Template) — lead with the fix and any action needed.
10. **Verify in the field**: confirm affected customers are unblocked. Update the incident record.

## Do / Don't

- **Do** keep the diff tiny and traceable to the incident.
- **Do** add the regression test that would have caught it.
- **Don't** touch EventLog, replay, storage, tax, posting, or compatibility.
- **Don't** ship without a green full-gate run, no matter how urgent.
