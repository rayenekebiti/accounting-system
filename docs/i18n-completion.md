# Internationalization Completion (Full Audit & Translation)

A quality/completeness pass that eliminated every untranslated **user-facing UI string** across the
application in **English, French, and Arabic**, and brought the translation catalogs fully in sync
with the source. **No architecture change:** still `qsTr()` + Qt Linguist `.ts`/`.qm` + the existing
translator loading, RTL support, and locale switching. No new framework, runtime dictionary, or JSON
language files. No accounting logic, storage format, replay semantics, or compatibility governance
changed; all regression gates stay green.

## Result at a glance

| Catalog | Messages | Unfinished | Plurals | Status |
|---------|:--:|:--:|:--:|--------|
| `app_en.ts` | 471 | 0 | 17 | source = translation (explicit) |
| `app_fr.ts` | 471 | 0 | 17 (2 forms) | fully translated |
| `app_ar.ts` | 471 | 0 | 17 (6 forms) | fully translated |

- **`tools/i18n-check.sh`: PASSED** — all 7 checks, including **#5 catalog freshness, which was the
  standing debt and is now zero** (471 messages vs 457 `qsTr` calls).
- Before this phase: 104 catalog messages vs 457 `qsTr` calls (78% of UI strings uncatalogued).

## Audit methodology

1. **Source audit (Phases 2–3).** Swept all `quick/qml/*.qml` for user-visible literals via
   `i18n-check.sh` check #1 (text/title/label/placeholder/message/… assigned a quoted literal) plus
   manual review of `Accessible.name`/`Accessible.description`/`ToolTip` and computed-text bindings.
   Also swept the C++ layer (`quick/*.cpp`) for `tr()`/`QObject::tr()`/`QCoreApplication::translate`
   that reaches the UI.
2. **Extraction (Phase 1/4).** Ran `lupdate` over `quick/qml` **plus** the five C++ ViewModels/models
   that legitimately host user-facing `tr()` text (see below), regenerating all three `.ts` with every
   current string. Revealed **453 → 471** unique sources across **40 contexts**.
3. **Translation (Phase 5).** Authored glossary-consistent FR + AR for every source (see glossary),
   with correct plural forms (FR 2, AR 6) and placeholder (`%1`/`%2`/`%n`) preservation.
4. **RTL + visual review (Phases 6/8).** Regenerated EN/FR/AR baselines and inspected every screen.
5. **Regression (Phase 9).** `i18n-check`, `itest`, `ptest`, `fuzz`, `ACCT_COMPAT_VERIFY` — all green.

## Screens inspected (all 40 contexts)

Invoices (+ editor, row, status badge), Customers (+ editor), Suppliers (+ editor), Payments (+ editor,
allocation editor), Expenses (+ editor), Ledger workspace (Accounts, Journal/LedgerExplorer, Journal
Entry inspector, Trial Balance, Tax Summary + tax-code editor), Settings workspace (General, Company,
Backup, Diagnostics, About), Recovery dialog + blocker, Nav rail, Language switcher, Search fields,
Empty states, Confirm/Discard dialogs, Modal sheet chrome. Every table header, filter chip, button,
tab, tooltip, validation error, status chip, empty state, and window/dialog title was verified.

## Files inspected / changed

- **Catalogs:** `quick/i18n/app_{en,fr,ar}.ts` — regenerated + fully translated.
- **Tooling:** `tools/i18n-extract.ps1` (now also scans the 5 C++ files below); `tools/i18n-check.sh`
  (added `⚠`/`✓` to the decorative-glyph allowlist, alongside the existing `✕`/`＋`/`🌐`);
  `tools/i18n_fill.py` (**new** — the authored FR/AR/EN translation data + injector, kept as the
  record of what was translated and how).
- **C++ `tr()` reachability fix (see below):** `quick/DiagnosticsViewModel.{h,cpp}`,
  `quick/SettingsViewModel.{h,cpp}`, `quick/BackupViewModel.h`, `quick/LedgerEntriesModel.cpp`,
  `quick/ExpenseEditorViewModel.{h,cpp}`, `quick/main_quick.cpp` (language-change wiring).
- **Enum-label display mapping (see below):** `quick/qml/{AccountsScreen,TrialBalanceScreen,
  TaxSummaryScreen,ExpensesScreen}.qml`.

## Untranslated strings found (and how each was fixed)

Three classes of gap were found beyond the obvious stale catalog:

1. **Uncatalogued QML strings (the bulk).** ~349 `qsTr` strings added across B2–C1 were never
   extracted. **Fixed** by re-running `lupdate` and translating all of them.

2. **C++ `tr()` that `lupdate` never scanned.** `lupdate` only scanned `quick/qml`, so user-facing
   text in five C++ ViewModels/models was uncatalogued **and** English-only:
   - `DiagnosticsViewModel` (engine health + verification result strings),
   - `BackupViewModel` (backup/verify/restore status + "No backups yet"),
   - `SettingsViewModel` (crash-recovery detail messages),
   - `LedgerEntriesModel` (`Entry #%1` / `Reversal of #%1` journal descriptions),
   - `ExpenseEditorViewModel` (combo labels `No tax` / `— none —` / category names).

   **Fixed** by (a) adding those five files to the `lupdate` scan; (b) reformatting the two stored
   result strings in `DiagnosticsViewModel` and the recovery detail in `SettingsViewModel` so they are
   **computed from state on read** (they were previously captured before the translator was installed);
   and (c) wiring each to `LocaleController::languageChanged` via a `retranslate()` re-emit — the C++
   analogue of `QQmlApplicationEngine::retranslate()`, so they refresh on a **live** language switch.
   `LedgerEntriesModel` now uses `tr()` (class context) instead of `QObject::tr()`.

3. **Enum keys rendered raw.** Several models return an English **key** used for filtering/logic
   (`typeName` Asset/Liability/Equity/Income/Expense; `normalSide` Debit/Credit; expense `category`
   Office/Rent/Utilities/Travel/Other; tax `typeName` VAT/GST/Sales Tax/Zero-rated/Exempt). These keys
   were shown directly as display text. **Fixed** by keeping the key English (filtering/tone unchanged)
   and mapping key→`qsTr` **at each display site** (per-screen `typeLabel`/`sideLabel`/`catLabel`/
   `taxTypeLabel` helpers that read `i18n.language` so they re-run on a live switch). The expense
   category combo is relabelled in `categoryOptions()` (now a `NOTIFY` property re-emitted on
   language change).

## Terminology consistency (glossary)

A single canonical term per concept is used everywhere (EN → FR / AR):

| EN | FR | AR |
|----|----|----|
| Invoice | Facture | فاتورة |
| Customer | Client | عميل |
| Supplier | Fournisseur | مورّد |
| Payment | Paiement | دفعة |
| Expense | Dépense | مصروف |
| Ledger | Grand livre | دفتر الأستاذ |
| Journal | Journal | دفتر اليومية |
| Trial Balance | Balance de vérification | ميزان المراجعة |
| Account | Compte | حساب |
| Debit / Credit | Débit / Crédit | مدين / دائن |
| Outstanding | Encours | المستحق |
| Settlement / Allocation | Règlement / Affectation | تسوية / تخصيص |
| Void | Annuler | إبطال |
| Reverse / Reversal | Contrepasser / Contrepassation | عكس / قيد عكسي |
| Posted | Comptabilisé | مُرحَّل |
| Draft | Brouillon | مسودة |
| Paid / Overdue | Payé / En retard | مدفوع / متأخر |

Professional accounting register was used throughout: French uses *contrepasser/contrepassation* for
reversals, *comptabilisé* for posted, *encours* for outstanding; Arabic uses Modern Standard Arabic
accounting terms (*قيد عكسي*, *مُرحَّل*, *ميزان المراجعة*, *الذمم*), correct plural morphology
(zero/one/two/few/many/other), and RTL-appropriate punctuation.

## RTL review

Verified across every Arabic screen (baselines `ar/standard/`): NavRail mirrors to the inline-end
(right); tab bars, headers, cards, chips, badges, table columns, and dialogs mirror correctly; the
focus/selection order is logical; no clipped or reversed text. Monetary amounts and identifiers stay
LTR (western tabular digits) inside RTL paragraphs by design (see `docs/i18n.md` §3), so columns remain
scannable. Arrows in localized strings point in the reading direction (`→` EN/FR, `←` AR). All
alignment is logical (`i18n-check` #7 green) — no `Text.AlignRight` RTL hacks.

## Dynamic strings (Phase 7)

Counts use `qsTr("%n …(s)", "", n)` with per-locale plural forms (FR 2, AR 6) — e.g. `%n facture` /
`%n factures`, and the six Arabic forms per noun. Placeholders are always inside the translatable unit
(`qsTr("Backup from %1").arg(x)`), never concatenated (`i18n-check` #6 green). Money/dates remain
locale-neutral western/ISO by the documented design.

## Screenshots

Regenerated `build/baselines/{en,fr,ar}/standard/*.png` and inspected manually. Confirmed, e.g.:
FR Invoices (Factures / Encours / Comptabilisé / Brouillon, plurals correct); AR Diagnostics (all
former C++ strings now Arabic, incl. the verification badges); AR Trial Balance & Accounts (type +
normal-side now أصل/التزام/مدين/دائن); AR Expense editor (— لا شيء — / بدون ضريبة / categories).

## Remaining limitations (honest)

- **System chart-of-account names are not translated** (Cash, Revenue, Accounts Receivable, Accounts
  Payable, Expenses, Tax Payable, Recoverable Tax). These are engine-authored **ledger data / stable
  role keys** persisted in the `AccountOpened` event log (the code comment states "Names are the stable
  role keys") and are used as lookup keys (`accountIdByName`, `setAccountScopeByName`) and posting
  identifiers. Localizing stored data values is a **data-localization** concern distinct from wrapping
  UI strings, and a naive display map would collide with identically-spelled UI labels (e.g. "Cash" the
  payment method → *نقدًا* vs "Cash" the account → *النقدية*). This is analogous to not translating
  customer/supplier names or invoice numbers. A future data-localization layer (keyed role → localized
  display, with disambiguation) could address it without touching storage.
- **A few engine version-contract identifiers on the Diagnostics screen** stay in a technical Latin
  form by design: the posting-policy tag `v2`, the version-contract line `schema 1 · replay 1 · …`, and
  the compatibility status token (`compatible`). These are governance identifiers, not prose.
- **Text input caret/IME/paste** in Arabic is Qt framework behaviour and still warrants a manual typing
  pass (cannot be screenshot-tested) — unchanged from prior phases.

## Verification

```
tools/i18n-check.sh    # PASSED — all 7 checks (freshness debt now ZERO)
tools/itest.sh         # 116 passed, 0 failed (incl. language-switching + settings-system)
tools/ptest.sh         # ALL PERSISTENCE TESTS PASSED
tools/fuzz.sh          # ROBUST
ACCT_COMPAT_VERIFY=1   # replay-equivalence held (full model + snapshot + trial balance)
tools/shots.sh         # EN/FR/AR baselines regenerated + visually verified
```

## Maintenance workflow (unchanged, now documented)

1. Add/adjust `qsTr` in QML (or `tr()` in one of the five scanned C++ files).
2. `pwsh tools/i18n-extract.ps1` (runs `lupdate` over `quick/qml` + the 5 C++ files).
3. Add the FR/AR (and EN) strings to `tools/i18n_fill.py`, then `python tools/i18n_fill.py`
   (or translate the `unfinished` entries by hand in Qt Linguist).
4. `bash tools/i18n-check.sh` → must pass. Rebuild (`lrelease` embeds the `.qm`).
