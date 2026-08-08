# Invoice Editor — Architecture & Reference Pattern Spec

The first real write path. Establishes the reusable patterns for forms, editing,
validation, saving, dialogs, keyboard workflow, and state lifecycle that ALL future
screens inherit. Backend/storage/core untouched. Optimize for an operator spending
hours in the app: fast, calm, predictable, keyboard-first.

## 0. Ownership boundary (the law)
- **C++ owns ALL edit state + logic**: buffer, validation rules + messages, totals math,
  money parsing, dirty tracking, save/commit. The `InvoiceEditorViewModel` is the single
  source of truth.
- **QML owns presentation only**: layout, focus, keyboard wiring, and *when* to reveal
  errors (touch/save timing). NO business logic, NO totals math, NO money parsing in QML.
- **Single direction of mutation**: QML input → VM setter / model setData → VM recomputes &
  validates → VM notifies → QML re-renders. Use `onTextEdited`/`onEditingFinished` (user-only),
  never `onTextChanged` (fires on programmatic set → loops). No duplicated state.

## 1. C++ — InvoiceDraftLinesModel (QAbstractListModel)
Editable working model of invoice lines (friendly units; converted to InvoiceLine at commit).
- Internal row struct: `{ QString description; double qty; double unitPrice; double taxPct; }`.
- Roles (editable via setData): `description, qtyText, unitPriceText, taxText`; read-only: `lineTotalText`.
- `roleNames()` → description, qtyText, unitPriceText, taxText, lineTotalText.
- `setData(idx, value, role)`: parse (qty/price/tax via toDouble, guard NaN/negative→0), store,
  `emit dataChanged(idx, idx, {LineTotalRole})`, `emit linesChanged()`. Return true.
- `Q_INVOKABLE void addBlankLine()` (append default {"",1,0,0}); `Q_INVOKABLE void removeLine(int row)`.
- `void setFromInvoiceLines(const std::vector<InvoiceLine>&)` (load on edit).
- `std::vector<InvoiceLine> buildLines() const` — qty→milliunits, price→Money, taxPct→permille,
  call recompute(); used at commit.
- `double subtotal() const`, `double taxTotal() const`, `double total() const` (sum over rows).
- `bool hasValidLine() const` (≥1 row with non-empty desc && qty>0 && unitPrice≥0).
- `int count` Q_PROPERTY (rowCount + NOTIFY on insert/remove/reset) for QML state.
- Signal `linesChanged()`.

## 2. C++ — InvoiceEditorViewModel (QObject) — THE reference VM
Header buffer (each: READ/WRITE/NOTIFY; setter sets value + dirty + revalidates that field):
- `QString invoiceNumber`
- `int customerId`            (-1 = none)
- `QString issueDate`         (ISO "YYYY-MM-DD")
- `QString dueDate`           (ISO)
- `int status`               (InvoiceStatus int; default INVOICE_DRAFT)
Read-only / derived:
- `InvoiceDraftLinesModel* lines` (CONSTANT pointer; listen to its linesChanged → recompute totals + dirty)
- `QVariantList customerOptions` (NOTIFY; [{value:id,label:name}] from StorageService, non-deleted)
- `QString subtotalText, taxText, totalText` (NOTIFY totalsChanged; "$%.2f" from lines model)
- `bool isNew` (NOTIFY)
- `bool dirty` (NOTIFY) — set true by any buffer/line mutation; false after begin*/commit
- `QString title` (NOTIFY) — isNew ? "New Invoice" : "Edit " + invoiceNumber  (QML may override via qsTr; VM exposes the number + isNew so QML builds the title — DO NOT tr in C++)
  → Actually expose `isNew` + `invoiceNumber`; QML composes the title string with qsTr.
Validation (VM computes WHAT is invalid; QML decides WHEN to show):
- Per-field error string props (NOTIFY validationChanged), empty = valid:
  `customerError, numberError, issueDateError, dueDateError, linesError`
  Rules: customer (customerId>=0); number (non-empty, ≤15 chars); issueDate (valid ISO);
  dueDate (valid ISO AND ≥ issueDate); lines (lines.hasValidLine()).
- `bool valid` (NOTIFY) = all errors empty.
- `bool showErrors` (NOTIFY) — false until a failed commit; reveals all field errors.
Lifecycle invokables:
- `beginNew()`: reset buffer — invoiceNumber = NumberingService::peekInvoiceNumber();
  customerId=-1; issueDate=today ISO; dueDate=today+30 ISO; status=Draft; lines: one blank line;
  load customerOptions; isNew=true; dirty=false; showErrors=false; revalidate.
- `beginEdit(int invoiceId)`: load Invoice via StorageService.invoices().load(id); populate buffer
  (number, customerId, issueDate/dueDate via IsoDate.toString, status); lines.setFromInvoiceLines(
  invoiceLines().findByInvoice(id)); load customerOptions; isNew=false; dirty=false; showErrors=false.
- `Q_INVOKABLE bool commit()`: revalidate all; if !valid → showErrors=true; emit validationFailed(firstInvalidFieldName); return false.
  Else build Invoice (number, customerId, IsoDate from ISO strings, Money subtotal/tax/total from lines,
  status). NEW: invoices().save(inv) → id; for each buildLines(): setInvoiceId(id), invoiceLines().save(line);
  then if invoiceNumber == peeked → NumberingService::reserveInvoiceNumber() (advance counter).
  EDIT: invoices().update(inv); for old in findByInvoice(id): remove(old.id); for new lines: setInvoiceId(id), save.
  Wrap in try/catch → on failure emit saveFailed(msg), return false. On success: dirty=false; emit saved(); return true.
- `Q_INVOKABLE void discard()`: emit discarded() (no storage touch).
Signals: dirtyChanged, totalsChanged, validationChanged, customerOptionsChanged, isNewChanged,
  saved(), discarded(), saveFailed(QString), validationFailed(QString firstField).
Wire as context property `invoiceEditor` in main_quick.cpp (single reused instance — no churn).
Add both .cpp to AccountingQuick sources.

## 3. C++ — InvoiceListModel: add id role
Add `InvoiceIdRole` → "invoiceId" (inv.getId()). So the list delegate can open the editor by id.

## 4. QML primitives (new/refactor — the FORM reference kit)
- **FieldInput.qml** (NEW, base input): bare token-styled TextField. Props: `text`(alias),
  `placeholder`, `hasError`(bool→red border), `horizontalAlignment`, `inputMethodHints`,
  `validator`. Focus ring (focusRing, 2px) / error (expense) / normal (border) border, radius md,
  Behavior on border.color. NO label. `font.family: Theme.font.uiFamily`. Signal `editingFinished`,
  `accepted`. This is the single input visual reused by forms AND line cells.
- **AppTextField.qml** (REFACTOR): label (+ required "*" in expense when `required`) + FieldInput +
  error text. FIX the stale `Theme.font.sans` → uiFamily. Props: label, placeholder, text(alias to
  FieldInput), error, required, horizontalAlignment, inputMethodHints. Forward editingFinished.
- **Select.qml** (NEW): labeled ComboBox. Props: label, model([{value,label}]), currentValue,
  error, required, placeholder. Token-styled (match FieldInput visuals: border, radius, focus ring).
  Emits `activated(value)`. RTL-safe (popup + text mirror). NO business logic.
- **ModalSheet.qml** (NEW, dialog shell): based on QtQuick.Controls.Basic `Popup`,
  `modal:true; dim:true; closePolicy: Popup.NoAutoClose` (Esc/scrim do NOT auto-close — routed to
  guard). Centered, width = Math.min(parent.width*0.92, 760), max height parent.height*0.9 with an
  internal Flickable/ScrollView for overflow. Structure: header (title Text + close IconButton) ·
  default content slot · footer slot (RowLayout for actions). Signals `requestClose()` (from Esc key,
  scrim click, close button). Property `title`. Dim scrim = canvas at ~50% (calm, not black). Rounded
  card (radius card), elevation 2 shadow. RTL: mirrors with app.
- **ConfirmDialog.qml** (NEW, minimal): small ModalSheet/Popup: message Text + two AppButtons
  (cancel ghost / confirm — variant configurable, e.g. danger for discard). Props: title, message,
  confirmText, cancelText, confirmVariant. Signals confirmed(), cancelled(). Reused for dirty-guard
  + future deletes.

## 5. QML — InvoiceEditor.qml (composition + keyboard + save flow)
A `ModalSheet` bound to `invoiceEditor`. Sections (progressive disclosure, grouped, calm):
- **Details** (2-col grid via GridLayout, columns:2): Customer (Select, required) · Invoice #
  (AppTextField, required, tabular) · Issue date (AppTextField ISO, required) · Due date
  (AppTextField ISO, required) · Status (Select). Labels qsTr.
- **Line items**: a column header row (Description | Qty | Unit Price | Tax % | Amount | ⌫), then a
  Repeater over `invoiceEditor.lines` (few rows; Repeater ok). Each row: FieldInput(description, fill)
  · FieldInput(qtyText, numeric, right/inline-end, ~64w) · FieldInput(unitPriceText, numeric, ~96w)
  · FieldInput(taxText, numeric, ~64w) · CurrencyAmount(amount: model.lineTotalText, ~96w) ·
  IconButton("✕", removeLine). Cells write via `model.<role> = text` onEditingFinished (controlled).
  Below: AppButton "＋ Add line" (ghost) → lines.addBlankLine() + focus new row's description.
- **Totals** (inline-end aligned block): Subtotal / Tax / **Total** (bold) via CurrencyAmount,
  tabular, column-aligned. Read-only.
- **Footer**: left/inline-start a muted dirty indicator ("Unsaved changes" when dirty); inline-end
  AppButton Cancel (ghost) + AppButton Save (primary). Save text = qsTr("Save").
Error reveal (the validation STANDARD): each field shows its VM error when (locally `touched` on
  editingFinished/focus-out) OR (`invoiceEditor.showErrors`). Pristine = no errors. Errors clear live
  as fixed. Save always enabled when dirty.
Save flow: Save / Ctrl+S / Ctrl+Return → `if (invoiceEditor.commit())` handled by VM `saved` →
  close + invoicesVm.refresh(); on validationFailed(field) → focus that field. Cancel / Esc / scrim /
  close → if `invoiceEditor.dirty` show ConfirmDialog (discard?) else close. On discard confirmed →
  invoiceEditor.discard() + close.
Keyboard STANDARD (encode here, inherited app-wide):
- Open → focus Customer (first field).
- Tab/Shift+Tab → next/prev logical field (declaration order; RTL uses logical order, not visual).
- Return in header field → next field (KeyNavigation or focus next). Return in a line cell → next
  cell; in last cell of last line → addBlankLine + focus new description; last cell of non-last line
  → next line's description.
- Ctrl+S / Ctrl+Return → Save. Esc → requestClose (dirty guard).
- addBlankLine → focus new description. validationFailed → focus first invalid.
Performance: single reused VM + single reused ModalSheet (open/close, not recreate). Line edits use
  setData/dataChanged (no model reset, no Repeater rebuild). No onTextChanged loops.

## 6. Wiring (Main.qml + InvoicesScreen.qml)
- InvoiceListModel: add invoiceId role; InvoicesScreen delegate `onClicked: root.rowActivated(model.invoiceId)`;
  change signal to `rowActivated(int invoiceId)`.
- Main.qml: instantiate one `InvoiceEditor { id: editor }`. InvoicesScreen
  `onNewInvoiceRequested: { invoiceEditor.beginNew(); editor.open() }`,
  `onRowActivated: (id) => { invoiceEditor.beginEdit(id); editor.open() }`.
  Connections on invoiceEditor.saved → { editor.close(); invoicesVm.refresh() }.

## 7. Financial formatting STANDARD
Money display via CurrencyAmount (tabular, numericFamily, 2dp, inline-end). Money INPUT: FieldInput
numeric, inline-end aligned, reformat to 2dp on editingFinished (QML may reformat display; canonical
value parsed in C++ at commit via Money — never float in storage). Invoice number tabular. Totals
column-aligned via tabular figures.

## 8. RTL / i18n
All labels/buttons/section titles/headers qsTr. Numerus where counts appear. Layout via GridLayout/
RowLayout + logical alignment → mirrors automatically. Money stays Western tabular digits, inline-end.
Tab order logical (unchanged in RTL). Validate in Arabic: mirrored form, inline-end amounts, ISO dates
unaffected, no truncation (let labels wrap / fields fill).

## 9. Verification (headless)
- VM probe (ACCT_PROBE2 or extend probe): beginNew(); set customerId, set a line qty/price via the
  lines model; assert subtotalText/totalText correct; assert valid toggles with required fields;
  commit() with missing customer → false + showErrors; then valid → commit true; reload storage →
  new invoice present with N lines. Write results to probe file.
- Build 0 errors; qmllint clean of real errors; editor QML loads (load probe); Widgets app builds.

## 10. Out of scope (avoid overengineering)
Calendar-popup date picker (ISO text now; note future); product-catalog line autofill; multi-currency;
async save (storage is sync/fast; reuse QtConcurrent pattern only if it ever blocks); undo/redo;
per-line tax-code tables. Keep v1 the clean reference.
