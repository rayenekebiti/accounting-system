# General Availability Review (Phase C5)

The evidence-based go-to-market assessment for Occountant 1.0. This phase changed **no** engine,
architecture, storage, or accounting code — it produced the launch artifacts and verified the
product against every production gate.

## Release recommendation

> ## READY FOR PILOT
>
> The **software** is production-quality and, on the evidence, GA-grade: all 12 production gates are
> green, the accounting engine is deterministic and hardened, and six realistic businesses complete
> their entire lifecycle without a single failed assertion. It is **not yet READY FOR GENERAL
> AVAILABILITY** only because the **commercial launch surface** — a production code-signing
> certificate, hosted updates, license-key issuance, and finalised legal/pricing — is not yet stood
> up. None of those are software defects.
>
> **Ship now to a small, supported pilot cohort. Promote to GA the moment the six-item launch
> punch-list (ship-checklist §warnings) is closed.**

This is the honest reading of the evidence: strong product, unfinished storefront.

---

## 1. Executive summary

Occountant 1.0 is a private, offline, small-business accounting application for Windows. Across five
prior phases it was built on a deterministic, event-sourced engine and then hardened, packaged,
internationalised, and acceptance-validated. In C5 we produced the customer/support/website
documentation, a deterministic multilingual screenshot set, and ran every production gate for the
record.

**The engine and app are GA-quality.** The evidence:

- **All 12 production gates green** (build, ptest, itest, fuzz, performance, commercial,
  compatibility, installer, i18n, acceptance, security, accessibility) — run 2026-08-07.
- **Acceptance:** 6 business personas (café, retail, freelancer, consultant, repair shop, clinic)
  each complete company→customers→suppliers→expenses→invoices+VAT→payments→reports→backup→restore-
  verify→license→update→verification. **150/150 assertions.**
- **Hardening (C4):** corruption of every config artifact degrades gracefully; thousands of restarts
  and repeated backup/restore/update/crash/upgrade cycles leak nothing and never lose data.
- **Integrity:** trial balance always balances; the entire history replays byte-identically; the
  engine refuses to run on inconsistent data.

**What's not ready is the storefront**, not the product: production signing, update hosting, key
issuance, and legal/pricing (see §5 Risk and the ship-checklist warnings).

## 2. Strengths

- **Correctness you can prove.** Double-entry integrity is enforced automatically and verifiable on
  demand; every figure traces to an immutable event. This is the product's spine and it is
  exhaustively tested (fuzz, crash-injection, replay-equivalence).
- **Data safety.** Crash-consistent writes, verified backups, staged crash-safe restore, and a
  recovery blocker that refuses suspect data. Repeated-operation reliability is proven, not assumed.
- **Genuine privacy.** No cloud, no account, no telemetry — a real differentiator against SaaS
  incumbents, and honestly reflected in the support bundle (diagnostics only) and privacy copy.
- **Professional release engineering.** One-command signed release, four update channels,
  compatibility governance, deterministic build provenance — the machinery to support customers over
  many versions already exists.
- **Internationalisation done properly.** EN/FR/AR with correct plurals and full RTL, gated by an
  automated i18n check.
- **Accessibility baseline.** Full keyboard operation, named accessible tree, visible focus, logical
  mirroring — with the automated audit green.
- **Honesty as a feature.** Public Known Limitations, a matching-the-product marketing voice, and a
  disciplined support/incident process.

## 3. Known limitations (product, 1.0)

Not defects — scope. Fully documented in `docs/user/known-limitations.md`:

- **Windows-only**, **single-computer / single-user** (offline-first by design).
- **No import** from spreadsheets or other tools.
- **No built-in invoice PDF / email delivery** — records/tracks invoices but doesn't yet produce a
  document to send the customer. *Highest-impact roadmap item.*
- **Limited statement export** (on-screen trial balance/ledger/VAT; no printable P&L/balance sheet
  yet).
- **No application-wide keyboard accelerators** (full keyboard operation works; Ctrl-N-style
  shortcuts are pending).

## 4. Remaining technical debt

- **Update signature v1 → v2.** Update payloads use HMAC with an embedded key (anti-corruption/
  casual-tamper). The asymmetric **Ed25519** upgrade is designed and call-site-compatible; do it
  before/with public update hosting.
- **Updater delivery.** Local-path source in v1; the HTTPS fetch is a documented, interface-
  compatible gap.
- **Reproducible builds.** BuildInfo *metadata* is deterministic (git-derived); byte-identical
  *binaries* would additionally need `-ffile-prefix-map` and a pinned toolchain.
- **Legacy Widgets target.** The old `AccountingSystem` (Qt Widgets) app still builds from the repo;
  it is not shipped and can be retired to reduce surface area.
- **Cosmetic:** four Trial-Balance labels use a double space (C4 LOW); pre-existing Qt 6.10
  `invalidateFilter` deprecation warnings in the filter proxies should migrate to
  `begin/endFilterChange`.

None of these affect correctness, data safety, or the shipped product's behaviour.

## 5. Risk assessment

| Risk | Level | Basis / mitigation |
|---|---|---|
| **Data integrity / correctness** | **LOW** | The most-tested surface: deterministic engine, fuzz + crash-injection + replay-equivalence + acceptance all green. The engine refuses suspect data rather than corrupt silently. |
| **Upgrade / recovery** | **LOW** | `installer-test` + `reliability` prove upgrades preserve data and downgrades are refused; recovery proven across real process kills. |
| **Trust / SmartScreen (unsigned installer)** | **MEDIUM** | An unsigned installer triggers Windows warnings and erodes trust at scale. *Mitigation:* obtain a code-signing cert (EV) — a pilot with manual, hand-held delivery avoids this until then. |
| **Update delivery not hosted** | **MEDIUM** | Customers can't self-update until HTTPS hosting is wired. *Mitigation:* pilot customers can be updated manually; close before GA. |
| **Support burden from missing invoice PDF/email** | **MEDIUM** | Some customers expect to send invoices from the app. *Mitigation:* set expectations via Known Limitations + pick a first-customer profile that invoices by other means; prioritise on the roadmap. |
| **Legal / pricing unset** | **MEDIUM** | Can't sell without finalised Terms/Privacy/pricing. *Mitigation:* legal review + pricing decisions (not engineering). |
| **Accessibility (spoken SR / extreme OS modes)** | **LOW–MEDIUM** | Automated audit green; manual SR pass pending. *Mitigation:* one-off manual pass on target hardware. |

**Overall:** technical risk is **LOW**; the residual risk is concentrated in the **commercial launch
prerequisites**, which a pilot-first rollout is specifically designed to absorb.

## 6. Recommended first release scope

Ship exactly what is proven, and nothing that isn't:

- **In:** Windows 10/11 desktop; single-user; invoicing with VAT; customers/suppliers; payments +
  allocation; expenses; VAT summary; full ledger + trial balance; verified backups + restore;
  diagnostics + support bundle; signed updates + channels; offline licensing; EN/FR/AR.
- **Out (state plainly):** invoice PDF/email, import, multi-user, cloud/sync, macOS, printable
  financial statements, keyboard accelerators.
- **Delivery for the pilot:** manually-delivered (or manually-signed) installer, direct support,
  manual update pushes — until hosting + signing are live.

## 7. Recommended first customer profile

The customers who get the most value and feel the limitations least:

- **Single-operator businesses** where the owner does their own books: **freelancers, consultants,
  sole traders**, and small **cash-based shops** (café/retail/repair) — exactly the personas the
  acceptance suite validates.
- Who **value privacy / offline** and are wary of subscription SaaS.
- Who **invoice by other means today** (or don't need to send polished PDFs from the app yet), so
  the missing invoice-delivery feature isn't a blocker.
- In **English-, French-, or Arabic-speaking** markets (including RTL).
- **Comfortable with a desktop app** and willing to keep backups.
- For the **pilot specifically:** a small cohort (≈5–20) who will give feedback and can be supported
  directly.

## 8. Recommended pricing tier assumptions

*(Assumptions to validate with the business — numbers are placeholders in `docs/website/pricing.md`.)*

- **One-time purchase per computer**, not a subscription — the anti-SaaS position is a core selling
  point. "You buy the software once; you never rent your own books."
- **Two editions**, matching the license model the app already reads:
  - **Personal** — freelancers/sole traders; the full engine.
  - **Business** — small businesses; adds priority support (and future business-oriented features).
- **Updates:** e.g. one year of updates included, optional renewal thereafter (keeps revenue without
  holding data hostage).
- **Free trial** with a grace period (already built) to de-risk purchase.
- Position price at small-business-tool level; the value story is privacy + one-time cost + provable
  correctness, not undercutting SaaS monthly fees.

## 9. Roadmap after GA (priority order)

1. **Invoice document generation + delivery** (PDF, print, email) — the top limitation and the most
   requested small-business need.
2. **Update hosting + v2 Ed25519 signing** — complete the self-update story securely.
3. **Data import** (CSV/spreadsheet, then other tools) — lower the switching cost.
4. **Formatted financial statements** (printable P&L / balance sheet).
5. **Keyboard accelerators** and continued **accessibility polish** (spoken SR pass, high-contrast).
6. **macOS build** — the second platform the product was always aimed at.
7. **Optional, privacy-preserving cloud backup** — opt-in, never mandatory, never changing the
   offline-first default.
8. Retire the legacy Widgets target; migrate the Qt 6.10 deprecations.

## 10. Conclusion

On the evidence, Occountant 1.0 is a correct, safe, private, and well-engineered accounting product
whose **software** is ready for real customers. The gap to General Availability is a short,
well-understood **commercial punch-list** (signing, hosting, licensing ops, legal), not a software
gap. The responsible path is a **supported pilot now** — validating real-world use and closing the
punch-list — followed by **General Availability** once those six items are done.

**Recommendation: READY FOR PILOT → GA on punch-list completion.**
