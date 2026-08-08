# Pilot Participation Agreement — Occountant (Template)

> **NOT LEGAL ADVICE.** This is a plain-language template for a controlled pilot. Every `[PLACEHOLDER]`
> must be set by the business, and the final document **must be reviewed by a lawyer** before use
> (ties into ship-checklist WARNING #4). It is deliberately short and honest — a pilot agreement, not
> a mass-market EULA.

The commercial mechanics referenced here live in `docs/commercial-pilot.md` (pricing hypothesis,
trial→paid flow, support workflow) and `docs/user/license-activation.md` (activation steps).

---

## Parties

- **Provider:** `[LEGAL ENTITY]` ("we", publisher of Occountant — currently `RIO&JHK Technologies Co.`
  per release metadata; confirm the legal name).
- **Pilot Customer:** `[BUSINESS NAME]` ("you"), business type `[freelancer / consultant / small shop
  / repair / service company]`.
- **Effective date / pilot period:** `[START]` to `[END]` (default `[SET length, e.g. 30–60 days]`).

---

## 1. What the pilot is

You agree to use Occountant for your **real** day-to-day accounting during the pilot period and to
give us honest feedback. In return you get the software free during the pilot and hands-on support.
The goal is mutual: you get a working tool and a voice in it; we learn whether it's ready and worth
paying for.

## 2. Your data is yours, and it stays with you

- Occountant runs **entirely on your computer**. Your accounting data (customers, invoices, payments,
  expenses, ledger) **never leaves your machine** through the software — there is no cloud, no
  account, no automatic upload, and no telemetry.
- You own your data at all times. You can export it (CSV / PDF) and back it up yourself.
- **Diagnostics you choose to send** (a "Support Bundle") contain **only technical health
  information** — build/version, redacted logs, crash info, integrity/compatibility reports — and a
  random, non-identifying **Support ID**. They contain **no accounting data** by design. Nothing is
  sent unless **you** send it.

## 3. Backups and your responsibility

- The software makes automatic and on-demand **backups**, and lets you verify and restore them.
- You are responsible for keeping your own copies (e.g. on your backup drive or storage) and for
  running backups per `docs/user/backup-guide.md`. We will show you how during onboarding.

## 4. Pilot software — no warranty, limited liability

- Occountant is provided **"as is"** during the pilot. While the accounting engine is extensively
  tested (balanced double-entry, crash-safe storage, verified backups), this is **early software** and
  you use it at your own risk for the pilot period.
- **You remain responsible for your books and filings.** Occountant is a tool to help you keep
  accounts; it is **not** an accountant and does not provide tax or legal advice. Verify figures with
  your accountant, especially at period/year end.
- To the extent permitted by law, our liability for the pilot is limited to `[PLACEHOLDER — e.g. the
  amount you paid, which during the pilot is zero]`. `[Lawyer to set the enforceable limitation.]`
- **If you ever see a wrong number or suspect data loss, stop and tell us immediately** (§7) — we
  treat that as the highest priority.

## 5. Support

- Direct, hands-on support during the pilot via `[SUPPORT CHANNEL — email / phone]`.
- Target response: acknowledge within `[SET, e.g. 1 business day]`; a correctness / data-safety /
  crash issue gets a same-day plan.
- To report an issue: quote your **Support ID** (Settings → Diagnostics) and, for bugs, attach a
  **Support Bundle** (Settings → Diagnostics → Create support bundle). See
  `docs/pilot-operation-manual.md` §7.

## 6. Feedback

- You agree we may **use your feedback** (bug reports, suggestions, usage observations) to improve the
  product, with **no obligation** on either side. We will not attribute quotes or publish your
  business name without your written permission.
- Feedback does not transfer any rights to your data or your business information — only to the ideas
  in your suggestions.

## 7. Price, trial, and buying after the pilot

- During the pilot the software is **free**. There is no obligation to buy.
- If you choose to continue after the pilot, Occountant is a **one-time purchase per computer** with
  optional yearly updates (see `docs/commercial-pilot.md`; price `[PLACEHOLDER]`, edition
  `[Personal / Business]`).
- Buying is **offline**: you receive a license key and paste it into **Settings → About** — your data
  is unchanged, the trial limit is simply lifted (`docs/user/license-activation.md`).
- We may offer pilot participants `[PLACEHOLDER — e.g. a discount / extended trial]` as thanks. `[Set
  or remove.]`

## 8. Confidentiality

- We keep your business information and anything we see in support confidential and use it only to
  support you and improve the product.
- You agree not to publicly benchmark or disclose non-public details of the pre-release software
  without our consent during the pilot. `[Lawyer to confirm scope.]`

## 9. Ending the pilot

- Either party may end participation at any time with `[SET, e.g. 7 days]` notice.
- On ending: you keep your data and your backups (they're on your machine). The trial license may
  stop working per its terms; **your books remain readable/exportable** while the trial is valid, so
  export what you need before it lapses if you don't continue.
- Uninstalling the software **keeps** your data directory (`docs/pilot-operation-manual.md` §5).

## 10. Governing terms

- This pilot agreement is governed by `[JURISDICTION]`.
- The full Privacy Policy and Terms of Use (`docs/website/privacy.md`, `terms.md`) apply once
  finalised; where they conflict with this pilot agreement during the pilot, `[specify which
  controls]`. `[Lawyer to finalise.]`

---

## Acceptance

| | Provider | Pilot Customer |
|---|---|---|
| Name | `[NAME]` | `[NAME]` |
| Title | `[TITLE]` | `[TITLE]` |
| Signature | ________________ | ________________ |
| Date | `[DATE]` | `[DATE]` |

---

### Operator note (not part of the agreement)
Keep this light. The pilot's purpose is to learn whether customers will keep using Occountant and pay
— a wall of legalese works against that. Get the data-ownership/privacy promises (§2) and the
"you're responsible for your books / no tax advice" points (§4) reviewed carefully; those are the
ones that matter most for an accounting tool. Set every `[PLACEHOLDER]` and have a lawyer pass before
first signature.
