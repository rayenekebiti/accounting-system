# Customers Workflow — Generalization Spec & Architecture Validation

Second fully-operational entity on the new architecture. Purpose: PROVE the
Invoices patterns generalize. NO architecture redesign. Reuse the established
laws + primitives; extract a standard ONLY where two real usages prove it.

## Architecture findings (the actual deliverable — validated while building)
1. **Editor VM skeleton separates cleanly from sub-models.** `CustomerEditorViewModel`
   is the SAME lifecycle as `InvoiceEditorViewModel` (beginNew/beginEdit/commit/discard,
   dirty, per-field errors + showErrors + touched gate + validationFailed) WITHOUT a
   lines sub-model, customerOptions, dates, or status. → the editor pattern is NOT
   invoice-coupled. The lines model was the only invoice-specific part.
2. **Progressive-validation framework is entity-agnostic.** Customer adds a NEW rule
   TYPE — email FORMAT (regex) — proving the framework handles more than required-field
   checks. Same machinery (error strings + valid + showErrors + firstInvalidField).
3. **Money-as-derived generalizes across derivation strategies.** Invoice total = computed
   from lines; customer balance = computed from invoices+payments. Both honor "C++ owns
   money"; balance is READ-ONLY in the editor (shown, never typed). 
4. **Per-id balance API doesn't scale to lists.** `computeCustomerBalance(id)` rescans all
   tables each call → O(n·m). FIX (done, in core): `StorageService::computeCustomerAggregates()`
   single-pass O(n+m+p) returning id→{balance,hasOverdue}. UI never re-derives money.
5. **FilterProxy is correctly per-entity, NOT shared yet.** InvoiceFilterProxy filters by
   status; CustomerFilterProxy filters by category (Owing/At-risk) + search. Structure is
   near-identical but the axis differs. Rule-of-three: a shared base is justified at entity
   #3, NOT now. Do not abstract speculatively.
6. **SummaryBar extraction IS justified (rule-of-two, identical structure).** Both screens
   wrap `Card { RowLayout { MetricCell · Divider · ... } }`. Extract `SummaryBar.qml` (Card
   shell + standard padding/spacing, children explicit) and refactor BOTH screens. This is a
   layout primitive (tier of ListRowCard), NOT a meta-generator.

## Metrics (honest, computable — NO invented/decorative metrics)
Customer has NO created-date field → "recently added / new this month" is NOT computable →
DROPPED (don't fake it). Three operationally-real AR-health cells:
- **Customers** (hero) — total non-deleted count · sub "%n owing"
- **Outstanding** — Σ positive balances (money, brand/neutral tone — NOT income-green; a
  receivable owed is not a gain) · sub "%n with balance"
- **At-risk** — count of customers with ≥1 overdue invoice (pending tone) · sub "overdue"
(3 cells, not 4 — "related but not cloned"; also proves SummaryBar isn't hardcoded to 4.)

## C++ layer (Dispatch A — mirror the Invoices trio)
### CustomerListModel (QAbstractListModel) — quick/CustomerListModel.{h,cpp}
Roles: customerId(int), name, email, phone, balanceText("$%.2f"), hasBalance(bool), atRisk(bool).
- refresh(): customers().loadAll() → rows; computeCustomerAggregates() ONCE → assign balance/
  atRisk per row (balance shown is the aggregate, not Customer.getBalance()). Skip deleted
  (loadAll already does). beginResetModel/endResetModel.
- roleNames per above. Expose `const std::vector<Customer>& customers()` + parallel aggregate
  vectors OR a row struct {Customer, double balance, bool atRisk}. Keep a row struct.

### CustomerFilterProxy (QSortFilterProxyModel) — quick/CustomerFilterProxy.{h,cpp}
- setCategoryFilter(QString): ""|"All"=no filter, "Owing"=hasBalance role true, "AtRisk"=atRisk true.
- setSearchText(QString): match name OR email OR phone (case-insensitive substring).
- filterAcceptsRow reads roles (O(1)/row — no money recompute).

### CustomersViewModel (QObject) — quick/CustomersViewModel.{h,cpp}
Mirror InvoicesViewModel. Props (NOTIFY summaryChanged unless noted):
- listModel (proxy, CONSTANT), totalCount, filteredCount (NOTIFY filteredCountChanged, wired to
  proxy rows signals — SAME robust pattern; QAbstractItemModel has no QML count).
- totalCustomers(int), owingCount(int), outstandingText(QString), withBalanceCount(int),
  atRiskCount(int).
- Q_INVOKABLE setCategoryFilter(QString), setSearchText(QString), refresh().
- Ctor takes CustomerListModel*; build internal proxy. recomputeSummaries() iterates the list
  model's rows (balance>0 → owing + Σ; atRisk → count).

### CustomerEditorViewModel (QObject) — quick/CustomerEditorViewModel.{h,cpp}
SAME skeleton as InvoiceEditorViewModel, header-only:
- Buffer props (READ/WRITE/NOTIFY, setter sets+dirty+revalidate): name, email, phone, taxNumber (all QString).
- Derived: balanceText (NOTIFY) — read-only, from computeCustomerAggregates()/computeCustomerBalance
  on beginEdit (0 for new); isNew(NOTIFY), dirty(NOTIFY).
- Validation (NOTIFY validationChanged): nameError (required, ≤31 chars), emailError (optional;
  if non-empty must match a basic email regex `^[^@\s]+@[^@\s]+\.[^@\s]+$` AND ≤47), phoneError
  (optional ≤15), taxError (optional ≤15). valid, showErrors(NOTIFY showErrorsChanged).
- beginNew(): clear buffer, balance "$0.00", isNew=true, dirty=false, showErrors=false, revalidate.
- beginEdit(int id): load customer, populate buffer, balanceText from aggregate, isNew=false,
  dirty=false, showErrors=false, revalidate. store editId_.
- commit()→bool: revalidate; if !valid → showErrors=true; validationFailed(firstField); return false.
  Else try { build Customer via CustomerData (name/email/phone/taxNumber; balance: NEW = Money(0);
  EDIT = keep existing customer's stored starting balance — load it, don't zero it). NEW:
  customers().save(c). EDIT: c.setId(editId_); customers().update(c). } catch → saveFailed; false.
  dirty=false; saved(); true.  ⚠ Same const-char* lifetime rule: hold QByteArray locals for
  name/email/phone/taxNumber before constructing CustomerData (toUtf8().constData() dangles).
- discard(). Signals: same skeleton (saved/discarded/saveFailed/validationFailed + *Changed).

### Wiring (main_quick.cpp)
Add CustomerListModel custModel; custModel.refresh(); CustomersViewModel customersVm(&custModel);
CustomerEditorViewModel customerEditor; context props "customersVm", "customerEditor". Add all
.cpp to AccountingQuick sources.

### Headless probe (extend ACCT_PROBE_WRITE block — it mutates storage)
- customersVm metrics: totalCustomers≥0, outstandingText sane.
- customerEditor.beginNew(); valid()==false (name empty); setName("Acme")→ valid()==true;
  set bad email "x@" → emailError non-empty, valid false; fix email → valid true; commit()==true;
  reload customers count +1.
- beginEdit(thatId); setPhone("123"); commit()==true; reload → phone persisted.
Write: `custTotal=<n> custOutstanding=<text> custValidEmpty=<b> custEmailRejected=<b> custNewCommitted=<b> custEditCommitted=<b> custAfter=<n>`.

## QML layer (Dispatch B)
### SummaryBar.qml (NEW, extracted) — Card shell + RowLayout
`Card { padding lg } RowLayout { id row; width parent.width; spacing lg }` with
`default property alias content: row.data`. Refactor InvoicesScreen's summary Card→SummaryBar
(keep objectName summaryCard on it for the probe). CustomersScreen uses it too.

### CustomersScreen.qml (NEW) — mirror InvoicesScreen structure, customer data
- PageHeader title qsTr("Customers") subtitle `qsTr("%n customer(s)","",customersVm.totalCount)`;
  action AppButton qsTr("New Customer") → newCustomerRequested().
- SummaryBar: 3 MetricCells (hero Customers + Outstanding + At-risk) per Metrics above, with
  Dividers between. Tones: Outstanding "" (neutral), At-risk pending if atRiskCount>0.
- FilterBar: search qsTr("Search customers…") → customersVm.setSearchText; chips All/Owing/At-risk
  (single-select, activeCategory) → customersVm.setCategoryFilter("All"|"Owing"|"AtRisk"). KEYS
  stay English; labels qsTr.
- List states (SAME 4-state machine, counts from customersVm.totalCount/filteredCount):
  empty (qsTr no customers + action), filtered-empty (qsTr no matches), populated ListView.
- Row delegate (ListRowCard, padding md): Avatar{name; diameter 32} (justified: customer identity
  anchor — the ONE place avatars earn their place; NOT replicated elsewhere) · Column(name bold md
  + email muted sm; if email empty show phone, else "—") · spacer · CurrencyAmount{amount balanceText}
  · optional small Badge tone "pending" text qsTr("Overdue") visible when atRisk. onClicked →
  rowActivated(model.customerId).
- signals newCustomerRequested(), rowActivated(int customerId).

### CustomerEditor.qml (NEW) — ModalSheet, reuse ALL form primitives + standards
- title isNew ? qsTr("New Customer") : qsTr("Edit %1").arg(customerEditor.name).
- Details: single-column (or 2-col) AppTextFields: Name(required), Email, Phone, Tax number.
  Each: text bound from VM, write on onEditingFinished, touched gate + fieldError (SAME progressive
  pattern). Email inputMethodHints Qt.ImhEmailCharactersOnly.
- A read-only Balance display row (qsTr("Balance") + CurrencyAmount{customerEditor.balanceText}) —
  muted, clearly non-editable (proves derived-money-shown-not-typed).
- footerData: dirty indicator + Cancel(ghost,requestClose) + Save(primary,trySave). SAME.
- Keyboard SAME standard: onOpened focus name; Enter advances name→email→phone→tax; Ctrl+S save;
  Esc→requestClose dirty-guard→ConfirmDialog discard. validationFailed→focus field. resetTouched().
- objectName "customerEditorRoot".

### NavRail.qml (NEW, minimal — required: 2 screens must coexist)
Slim left sidebar, RTL-mirrored (logical anchors / Layout). `property string current; signal
navigate(string key)`. Two items now: {key:"invoices",label qsTr("Invoices"),glyph "🧾"},
{key:"customers",label qsTr("Customers"),glyph "👤"}. Selected item: brandSubtle bg + brand text;
others textSecondary, hover surfaceMuted. Width ~200 (or icon-only ~64 — choose ~180 with label).
Tokens only. NOT a router framework — a simple list + signal.

### Main.qml wiring
- Left NavRail + a StackLayout (currentIndex from a `property string currentScreen: "invoices"`):
  index0 InvoicesScreen, index1 CustomersScreen. BOTH stay alive (no churn). NavRail.onNavigate
  sets currentScreen. Map screen→index.
- InvoiceEditor + CustomerEditor both instantiated once.
- CustomersScreen onNewCustomerRequested: { customerEditor.beginNew(); custEditor.open() };
  onRowActivated:(id)=>{ customerEditor.beginEdit(id); custEditor.open() }.
- Connections customerEditor.onSaved: { custEditor.close(); customersVm.refresh() }.
- Keep LanguageSwitcher (move into NavRail bottom or keep corner — keep corner for now).
- RTL: NavRail on inline-start (mirrors to right in AR) via Layout order (no hardcoded left).

## Verification
- Build 0 errors; qmllint clean of real errors. 
- Read-only probe (ACCT_PROBE): both editors instantiate + OPEN (invokeMethod open → visible/height
  sane) — add customerEditorRoot open check. RTL layoutDir=1 still. 
- Write probe (ACCT_PROBE_WRITE): the custom metrics + validation + commit round-trip line above.
- Widgets app (AccountingSystem) still builds.
- Dataset stress (note in report): long names elide (Text elide), missing email shows phone/"—",
  RTL/Arabic names render inline-start, mixed LTR/RTL no scramble (names are plain Text, dir auto),
  large lists: ListView virtualizes + balances precomputed once (O(n+m), not per-row).

## Out of scope (no overengineering)
Generic FilterProxy base (entity #3), meta-form generator, customer merge/import, soft-delete UI
(remove() exists in repo but no delete button this phase — keep create/edit only, matching the
invoice editor's scope), avatar color theming beyond existing hash, pagination (virtualization
suffices now).
