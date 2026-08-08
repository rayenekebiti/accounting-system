# Commercial Preparation — Occountant Pilot (Phase C9)

Commercial groundwork for the first paying pilot. The goal is **not** marketing optimization — it is
to **prove retention first**: that a non-technical owner installs Occountant, does daily accounting
unaided, trusts the data, keeps using it after the trial, and that *some* will pay. This doc states
the pricing **hypothesis**, the trial→paid flow, the customer FAQ pointers, the support workflow, and
where onboarding lives. Everything here is a hypothesis to test with the pilot, not a launch plan.

Related, already-written material this ties together:
- Pricing copy (placeholders): `docs/website/pricing.md`
- Customer FAQ: `docs/user/faq.md` · License activation: `docs/user/license-activation.md`
- Support/ops set: `docs/support/` (triage, incident playbook, hotfix, rollback)
- Onboarding: in-app wizard + `docs/user/quick-start.md`

---

## 1. Pricing hypothesis (to validate, not to announce)

Occountant's model is a **one-time purchase per computer** with **optional yearly updates** — no
subscription to your own books, no cloud fees, no lock-in (the engine is fully offline). This matches
the licensing the app already enforces (offline key, per-machine, editions read from the key).

**Hypothesis for the pilot** (numbers are placeholders for the commercial owner to set):

| Edition | Who | One-time price (hypothesis) | Includes |
|---|---|---|---|
| **Personal** | Freelancers & sole traders | `[SET]` | Invoicing, payments, expenses, VAT summary, full ledger, backups, EN/FR/AR |
| **Business** | Small businesses | `[SET]` | Everything in Personal + priority support |

- **Updates:** first year included, optional renewal after — `[SET]`.
- **Trial:** free, full-feature, offline, no account, no card — `[SET length]` (the app issues a
  usable trial on first run; a grace window keeps a just-expired trial working briefly).

**What the pilot must answer before any public price is fixed:**
1. Does the daily workflow deliver enough value that owners would pay *anything*? (retention first)
2. Which edition split matches how they actually use it?
3. Is one-time-per-machine the right shape, or do they expect a subscription/multi-device deal?
4. What update-renewal terms feel fair?

Do **not** run price experiments during the pilot beyond confirming willingness-to-pay exists. First
prove they keep using it.

---

## 2. Trial → paid conversion flow

The mechanism already exists (LicenseManager: offline trial, editions, grace, tamper→Invalid,
corrupt-cache ignored — all `c2test`-covered). The **flow** to run:

1. **Day 0 — Install & trial starts automatically.** First run issues a full-feature trial; the
   onboarding wizard captures company identity. No account, nothing leaves the machine.
2. **During trial — value, not nagging.** The app works fully. Trust panel (Ledger → Trust) shows the
   books are balanced, verified, and backed up — the confidence that earns a purchase.
3. **Near expiry — one honest prompt.** Surface remaining trial time and how to buy/activate
   (Settings → About → License activation). No dark patterns, no feature crippling mid-task.
4. **Purchase — out-of-app, offline activation.** The customer buys a key (commercial/ops pipeline —
   §4 below); they paste it into **Settings → About**; the edition unlocks. No connection required.
5. **Post-purchase — data continuity.** Nothing about their books changes on activation — same files,
   same history. Buying removes the trial limit, not their data.

**Conversion is measured by retention + activation**, per pilot user: did they keep entering real
transactions through the trial, and did they activate a key at the end? Track both in the feedback
log (`docs/customer-feedback.md`).

**Gap to close before charging money (commercial/ops, not code):** the **key issuance/sales
pipeline** — minting and delivering license keys at point of sale. The app already *validates* keys;
someone must *sell* them. For a small pilot this can be manual (mint a key with the vendor tool,
email it). Ship-checklist WARNING #3 (production code-signing cert) and #4 (finalised legal/pricing)
also apply before taking public payment.

---

## 3. Customer FAQ (pilot-facing)

The full FAQ is `docs/user/faq.md`. The questions a paying pilot user asks first — answer these up
front in the handover:

- **"Is my data private?"** Yes — everything is on your computer. Nothing is uploaded; there is no
  cloud. Support bundles you send contain diagnostics only, never your accounting data.
- **"Will I lose my books if the app crashes / I uninstall?"** No. The app recovers on restart from a
  verified journal; uninstalling removes the program but **keeps** your books and backups.
- **"How do I get my numbers to my accountant?"** Export CSV (trial balance, P&L, VAT) and PDF
  invoices; per-customer statements and an outstanding-balances summary.
- **"Do I need internet?"** No — install, trial, use, back up, and activate a purchased key all work
  offline.
- **"What if the numbers are wrong?"** They're derived from an always-balanced double-entry engine;
  the Trust panel lets you verify the books on demand. If you ever see a wrong number, that's a
  class-A report we fix immediately (`docs/customer-feedback.md`).
- **"Which languages?"** English, French, and Arabic (right-to-left), switchable anytime.

---

## 4. Support workflow

Full set in `docs/support/`. The pilot loop:

1. **Intake** — user reports via the agreed channel (email/phone). For any bug, ask for a
   **Support Bundle** (Settings → Diagnostics → Create Support Bundle): build info + logs + crash
   reports, **no accounting data** (`security-gate.sh`-proven). Deterministic, offline.
2. **Triage** — classify with `docs/customer-feedback.md` (A–E) and severity; log the row. Use
   `docs/support/issue-triage-guide.md`.
3. **Correctness / data-safety / crash** → immediate; follow `docs/support/incident-playbook.md`;
   fix only through the full battery; ship via `docs/support/hotfix-procedure.md`
   (`docs/support/rollback-procedure.md` if a fix regresses).
4. **Everything else** → batch for weekly triage; missing-feature requests wait for multi-user
   evidence (§ decision rules in the feedback doc).
5. **Close the loop** — tell the user what happened; a fixed issue cites the gate/commit that now
   covers it.

**SLA hypothesis for the pilot** (set with the commercial owner): acknowledge within `[SET]`; a
class-A/data-safety/crash gets a same-day plan. Small pilot = high-touch, direct-to-engineer support
is fine and desirable (it *is* the feedback channel).

---

## 5. Onboarding documentation

- **In-app:** the first-run **onboarding wizard** (business name/address/tax number/currency/
  fiscal-year/language) sets up a complete company profile before the user enters the app — writing
  **settings only, authoring no accounting events** (verified). Localized + RTL-correct. Only the
  business name is mandatory.
- **Written:** `docs/user/quick-start.md` (first invoice → payment → report in minutes),
  `docs/user/user-guide.md`, `docs/user/backup-guide.md` + `restore-guide.md`,
  `docs/user/license-activation.md`. Include these in every handover (see
  `docs/pilot-release-guide.md` §7).
- **Handover ask:** walk the owner through their **first real invoice → payment → export** on the
  first call, and show them **Back Up Now** + where backups live. The one habit that builds trust is
  seeing a backup made and knowing it can be restored.

---

## 6. What we are deliberately NOT doing yet

- No marketing funnel, ads, SEO, or price A/B testing — **retention proof comes first.**
- No subscription billing, no accounts, no telemetry/analytics phoning home (privacy is a selling
  point — keep it true).
- No public update hosting or auto-update-as-a-security-claim until update signing is on the
  production key + HTTPS hosting (asymmetric mechanism is done — see `docs/update-signing.md`).
- None of the excluded product directions (inventory, POS, cloud sync, AI, ERP).

**Success bar for this commercial phase:** a handful of pilot businesses install unaided, run their
real daily books for the trial, trust the data, keep going after it ends, and at least some paste in a
paid key. That — not a polished funnel — is what tells us Occountant is commercially real.
