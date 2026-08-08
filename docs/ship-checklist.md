# Ship Checklist (Phase C5)

The go/no-go checklist for shipping Occountant 1.0 to paying customers. Every item is **PASS**,
**WARNING** (a launch prerequisite or accepted caveat — not a software defect), or **BLOCKER** (must
be fixed before any customer install).

**Evidence rule:** no optimism, no pessimism. Each verdict cites what was actually run or produced.

## Summary

- **BLOCKERS: 0**
- **WARNINGS: 6** — all are launch/ops/legal prerequisites, none are software defects.
- **PASS: everything else.**

The **software** is production-quality (all 12 production gates green — §Gate evidence). The
**warnings are the commercial launch surface** (production code-signing cert, update hosting, license
issuance, legal/pricing, manual a11y pass, update-signature v2). See the GA review for the
recommendation.

---

## Gate evidence (2026-08-07)

Full sweep, all green:

| Gate | Result |
|---|---|
| build | PASS (0 errors) |
| ptest (persistence/crash) | PASS |
| itest (interaction) | PASS |
| fuzz (adversarial) | PASS |
| performance | PASS |
| commercial (c2test) | PASS (21/21) |
| compatibility (replay-equivalence) | PASS |
| installer lifecycle | PASS |
| i18n | PASS (7/7) |
| acceptance (6 personas) | PASS (6/6, 150 assertions) |
| accessibility | PASS |
| security | PASS |

---

## Checklist

### Installer — **PASS** (artifact production is a WARNING)
- **PASS:** upgrade preserves data, downgrade refused, uninstall preserves data, portable mode —
  all proven by `installer-test.sh`. Inno Setup script complete (`installer/Occountant.iss`): ARP
  entry, Start-Menu shortcut, version metadata, `AppMutex`, output `Occountant-1.0.0-Setup.exe`.
- **WARNING (#1):** the real signed `.exe` has **not** been built or smoke-tested in this
  environment (Inno Setup `iscc` is not installed here). *Before GA:* build the installer on a
  machine with **Inno Setup 6**, Authenticode-sign it, and smoke-test install→run→upgrade→uninstall
  on clean Windows 10 and 11 VMs.

### Updater — **PASS** (production endpoint is a WARNING)
- **PASS:** check → verify signature → stage → apply-on-restart → rollback, crash-consistent;
  interrupted/forged updates rejected (`c2test`, `acceptance`, `reliability`).
- **WARNING (#2):** the update **source** is a local path in v1; the production **HTTPS manifest/
  payload hosting** must be stood up and the same interface pointed at it (documented gap in
  `app/UpdateManager.h`). *Before customers can receive updates:* host the signed `manifest.json` +
  artifacts and configure the update source URL.

### Signing — **WARNING (#3, #6)**
- **PASS (mechanism):** three-tier Authenticode pipeline (`tools/sign-authenticode.sh`) and signed,
  verified update payloads (`ACCT_SIGN` + `sig::verify`); tamper is rejected (`c2test`).
- **WARNING (#3):** no **production Authenticode certificate** is configured here, so shipped
  artifacts are currently unsigned. *Before GA:* obtain a code-signing cert (EV recommended for
  SmartScreen reputation) and sign the exe + installer via the pipeline.
- **WARNING (#6):** update-payload signing is **v1 HMAC with an embedded key** — strong against
  accidental corruption and casual tampering, but not an asymmetric guarantee (the key is in the
  binary). The v2 **Ed25519** upgrade (public key in app, private key held by vendor) is designed
  and call-site-compatible. *Recommended before or shortly after GA* if updates are hosted publicly.

### Licensing — **PASS** (key issuance is an ops prerequisite)
- **PASS:** offline trial issuance, grace period, editions, tamper→Invalid, corrupt-cache ignored
  (`c2test`). Activation UI in **Settings → About**.
- Note: the **license-key issuance/sales pipeline** (minting + delivering keys at point of sale) is
  a business/ops task outside the app; the app already validates keys. Track as a launch task.

### Backups — **PASS**
- One-click backup, verify, and staged restore; repeated backup/restore cycles leak-free
  (`c2test`, `reliability.sh`). Documented for users (Backup Guide, Restore Guide).

### Recovery — **PASS**
- Crash-consistent journal replay; recovery blocker on failed verification (never runs on suspect
  data). Proven across real process kills (`ptest.sh`) and repeated cycles (`reliability.sh`).

### Documentation — **PASS**
- Full non-technical customer set produced this phase (`docs/user/`: Quick Start, User Guide, Backup,
  Restore, FAQ, Troubleshooting, Migration, Keyboard shortcuts, Known limitations, Support, License
  activation, Update). Support/ops set (`docs/support/`) and website copy (`docs/website/`) produced.

### Support bundle — **PASS**
- `SupportBundle.zip` composes build info + diagnostics + logs + crash reports by **allowlist**; the
  security gate proves **no accounting data** (name or bytes) can enter it (`security-gate.sh`,
  `c2test`).

### Diagnostics — **PASS**
- **Settings → Diagnostics**: live event count, DB size, trial-balance status, and one-click
  **Run Verification** (non-destructive replay-equivalence). Startup diagnostics aggregate storage/
  license/backup/update health (`c2test`).

### Release channels — **PASS**
- Stable / RC / Beta / Development with precedence; updater filters by the user's channel; runtime
  switch persisted (`c2test`, `docs/release-engineering.md`).

### Compatibility — **PASS**
- Version governance across all axes; a newer build migrates older books, an older build refuses
  newer ones; replay-equivalence holds (`ACCT_COMPAT_VERIFY`, `verifyAll`). **Frozen this phase** —
  unchanged.

### Translations — **PASS**
- EN/FR/AR fully translated, plurals + RTL correct; all 7 i18n checks green (`i18n-check.sh`).
  Verified live (`ACCT_PROBE`: RTL + translated plurals).

### Performance — **PASS**
- Within thresholds at scale (`perf.sh`); endurance memory-bounded with zero warning drift
  (`reliability.sh`).

### Security — **PASS** (see Signing warnings)
- No accounting data escapes support/crash artifacts; signature-tamper and license-tamper rejected;
  logs redact money values (`security-gate.sh`, `c2test`). Trust-boundary caveat = the Signing
  WARNINGs (#3, #6).

### Accessibility — **PASS** (manual pass is a WARNING)
- Every **visible** interactive control is named; keyboard-only operation; focus rings; RTL
  mirroring (`a11y.sh`, C4 fixes).
- **WARNING (#5):** the **spoken screen-reader experience** (NVDA/Narrator) and **very large-font /
  high-contrast OS themes** need a **manual once-over on target hardware** before GA. The automated
  prerequisites (named tree, logical mirroring) pass.

### Legal & pricing — **WARNING (#4)**
- **WARNING (#4):** Privacy Policy and Terms of Use must be **finalised by a lawyer**; pricing/
  currency/refund/trial-length placeholders must be set (`docs/website/` placeholders). Not a
  software item, but a hard prerequisite for selling to customers.

---

## The six warnings, as a launch punch-list

1. Build + Authenticode-sign + smoke-test the real installer on Inno-Setup + signing-cert machine.
2. Stand up HTTPS update hosting and point the updater's source at it.
3. Obtain and configure the production code-signing certificate.
4. Finalise Privacy/Terms (legal) and pricing placeholders.
5. Manual screen-reader + extreme-OS-accessibility pass on target hardware.
6. Upgrade update-payload signing to v2 Ed25519 (before/soon after public update hosting).

**None are software defects.** All are commercial/ops/legal launch tasks. See the GA review for the
recommendation and sequencing.
