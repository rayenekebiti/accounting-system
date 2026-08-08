# Phase: Internationalization & RTL — Implementation Spec

Production-grade EN/FR/AR with Arabic as a first-class concern, on the existing
Qt Quick `App` module + C++ view-models. Backend/storage untouched. Pragmatic, not
over-engineered: Qt Linguist + retranslate(), no custom i18n framework.

## Core architectural rules (apply to ALL current and future screens)
1. **String ownership:** C++ view-models expose DATA and language-neutral KEYS only.
   ALL user-facing text is `qsTr()` in QML. Rationale: `QQmlApplicationEngine::retranslate()`
   refreshes only QML qsTr bindings; a C++ `tr()` property would go stale on language switch.
2. **Status (and similar enums):** English key in C++ (drives logic/tone/filter); QML
   translates for DISPLAY via literal `qsTr()` per known value (so lupdate can extract).
3. **Pluralization:** always `qsTr("%n thing(s)", "", count)` (Qt numerus → Arabic's 6 forms).
   Never `qsTr("%1 things").arg(n)`.
4. **Money:** Western digits, fixed tabular format, ALWAYS (even Arabic UI). Fixed numeric
   font, explicit right-align that does NOT mirror. Accounting scanning > locale purity.
5. **Identifiers & amounts** are LTR tokens; keep them from reordering in RTL paragraphs.
6. **RTL:** driven by language. `LocaleController` sets `QGuiApplication::layoutDirection`;
   QML root binds `LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft`,
   `childrenInherit: true`. Use Layouts + logical anchors only; never hardcode left/right.
7. **Tokens only** (existing rule). Fonts now come from `Theme.font.uiFamilies` (locale-aware
   list) and `Theme.font.numericFamilies` (fixed). No hardcoded family strings.

## Typography decision (now vs later)
- CORRECTION: QML's font value type exposes only `font.family` (single string), NOT
  `font.families`. So tokens are SINGLE locale-aware families and fallback is provided
  in C++ via `QFont::insertSubstitutions` (in main_quick.cpp):
  - `Theme.font.uiFamily` = rtl ? "IBM Plex Sans Arabic" : "Inter"
  - `Theme.font.numericFamily` = "Inter"
  - Substitutions: Inter→{Segoe UI,Arial}; IBM Plex Sans Arabic→{Segoe UI,Tahoma}.
  - Components use `font.family: Theme.font.uiFamily` (CurrencyAmount: numericFamily).
- Segoe UI renders Latin AND Arabic on Windows today → no font bundling required this phase.
- Bundling Inter + IBM Plex Sans Arabic is a later packaging-phase drop-in (fonts named first
  in the lists → auto-picked once added to resources + QFontDatabase). Document, don't build.
- Arabic line-height: `Theme.font.lineHeightArabic ≈ 1.35` vs Latin `≈ 1.20`; apply to
  multi-line/body text only (not single-line labels/amounts).

## C++ : LocaleController  (new: quick/LocaleController.{h,cpp})
- `QObject`, constructed with `QQmlApplicationEngine*` (for retranslate).
- `Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)` — "en"|"fr"|"ar".
- `Q_PROPERTY(bool rtl READ rtl NOTIFY languageChanged)` — derived: language=="ar".
- `Q_INVOKABLE QString displayName(lang)` → "English"/"Français"/"العربية".
- `setLanguage(code)`:
  - `qApp->removeTranslator(&translator_)`; `translator_.load("app_"+code, ":/i18n")` (qm in resource);
    if loaded `qApp->installTranslator(&translator_)`.
  - `QGuiApplication::setLayoutDirection(code=="ar" ? Qt::RightToLeft : Qt::LeftToRight)`.
  - `QSettings().setValue("ui/language", code)`.
  - `engine_->retranslate();`  emit `languageChanged()`.
- Constructor: read saved language (QSettings "ui/language"), else default "en"; apply it.
- Wire in `main_quick.cpp` AFTER engine creation: `LocaleController locale(&engine);`
  context property `"locale"`. Apply saved language before/after first load (after load, then
  retranslate is fine). Set initial layoutDirection too.

## main_quick.cpp font setup
- Set app default font families: `QFont f = qgapp.font(); f.setFamilies({"Inter","Segoe UI"}); qgapp.setFont(f);`
- (No font files to load this phase; fallback handles it.)

## Theme.qml additions
- Root: `property bool rtl: false` (Main keeps it synced to `locale.rtl`).
- In `font` group add:
  - `readonly property var uiFamilies: theme.rtl ? ["IBM Plex Sans Arabic","Segoe UI","Tahoma"] : ["Inter","Segoe UI","sans-serif"]`
  - `readonly property var numericFamilies: ["Inter","Segoe UI"]`
  - `readonly property real lineHeightArabic: 1.35`
  - `readonly property real lineHeightLatin: 1.20`
  - (give Theme root `id: theme`)
- Keep existing `sans`/`arabic` string tokens for reference; components migrate to the lists.

## Component migration (mechanical)
- Every component currently using `font.family: Theme.font.sans` → `font.families: Theme.font.uiFamilies`.
- `CurrencyAmount`: `font.families: Theme.font.numericFamilies` (NOT uiFamilies). Keep AlignRight,
  keep tnum. Ensure it renders LTR (pure ASCII; fine).
- `EmptyState` description (multi-line): add `lineHeight: Theme.rtl ? Theme.font.lineHeightArabic : Theme.font.lineHeightLatin; lineHeightMode: Text.ProportionalHeight`.

## StatusBadge.qml — translated display
- Keep `status` (English key) for tone mapping.
- Add `text` shown = a `_label` function with LITERAL qsTr per value:
  `function statusLabel(s){ switch(s){ case "Draft": return qsTr("Draft"); case "Posted": return qsTr("Posted"); case "Paid": return qsTr("Paid"); case "Overdue": return qsTr("Overdue"); case "Void": return qsTr("Void"); default: return s } }`
  Badge text binds to `statusLabel(status)`. Tone still from English `status`.

## InvoicesViewModel changes (C++)
- Add `Q_PROPERTY(int awaitingCount READ awaitingCount NOTIFY summaryChanged)` (= awaitingCount_).
- REMOVE `summaryLine` Q_PROPERTY + getter (moves to QML). Keep counts/sums (they're data).
- (Money sum strings stay as-is — Western, language-neutral, fine.)

## InvoicesScreen.qml audit fixes
- PageHeader subtitle: build in QML →
  `subtitle: qsTr("%n invoice(s)", "", invoicesVm.totalCount) + " · " + qsTr("%n awaiting payment", "", invoicesVm.awaitingCount)`
- StatCard captions: `qsTr("%n invoice(s)", "", invoicesVm.outstandingCount)` etc. (numerus).
- StatCard titles, EmptyState strings, chip labels, "New Invoice", "Search invoices…": ensure `qsTr`.
- Chips: DISPLAY `qsTr("Draft")` etc.; KEY stays English literal in setStatusFilter("Draft") and
  activeStatus comparisons (already correct — keep English keys).
- Status badge now translated via StatusBadge change (no screen change needed).
- Amounts already AlignRight + numeric font (via CurrencyAmount) — verify not mirrored.

## Language switcher (new: quick/qml/LanguageSwitcher.qml)
- Compact, calm. A small popup `Menu` triggered by an `IconButton` showing a globe "🌐" (or the
  current language short code "EN"/"FR"/"ع"). Menu items: English / Français / العربية, each
  `onTriggered: locale.setLanguage("en"|"fr"|"ar")`. Current language checkmarked.
- Place in Main.qml header area (replaces the dev RTL toggle).

## Main.qml
- Root window: `LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft;
  LayoutMirroring.childrenInherit: true`.
- `Component.onCompleted` / binding: keep `Theme.rtl` synced: `Binding { target: Theme; property: "rtl"; value: locale.rtl }` (or `Connections` on locale.languageChanged setting Theme.rtl).
- Replace dev RTL toggle with `LanguageSwitcher`.
- Keep hosting `InvoicesScreen`.

## CMake
- `find_package(Qt6 ... LinguistTools)` (add component).
- Create `quick/i18n/app_en.ts`, `app_fr.ts`, `app_ar.ts` (empty skeletons ok; lupdate fills).
- Use `qt_add_translations(AccountingQuick TS_FILES quick/i18n/app_en.ts quick/i18n/app_fr.ts
  quick/i18n/app_ar.ts RESOURCE_PREFIX "/i18n")` (Qt 6.10 signature; adjust if needed). This
  auto-creates `.qm` at build embedded under `:/i18n`, and an `update_translations`/lupdate target.
- Add `quick/LocaleController.cpp` to AccountingQuick sources; LanguageSwitcher.qml + any new qml to QML_FILES.

## Translation extraction & content
- Run lupdate (the generated `..._lupdate` or `update_translations` target, or call
  `/c/msys64/ucrt64/bin/lupdate.exe` on the qml+cpp) to populate the 3 `.ts`.
- Fill `app_fr.ts` (French) and `app_ar.ts` (Arabic) with real translations incl. plural forms.
  English `.ts` = source (can stay untranslated / identity).
- Build runs lrelease → `.qm` embedded.

## Verification (headless-friendly)
- Extend ACCT_PROBE: with probe, after load, programmatically `locale.setLanguage("ar")`, then write
  to the probe file: current language, `QGuiApplication::layoutDirection()` (1=LTR/2=RTL... use the
  enum int), and a couple of `QCoreApplication::translate` lookups (e.g., translate "Paid" and
  "%n invoice(s)") to PROVE the AR .qm loaded and plural works. Then switch back. This gives
  headless proof of: translator load, retranslate path, RTL flip — without needing a screenshot.
- Also: build 0 errors; qmllint clean of real errors (known missing-property/unqualified ok);
  runs alive in default language; Widgets app still builds.

## Out of scope (explicitly, to avoid overengineering)
- Font file bundling (later packaging phase; fallback families cover it now).
- Localized month-name dates / Hijri calendar (later).
- Per-currency symbol/placement localization (money stays current format).
- Any backend/storage changes.
