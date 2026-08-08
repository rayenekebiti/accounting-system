# QML / C++ Interop Patterns — Non-Negotiable Rules

Hard-won rules from real regressions where **screenshots passed, C++ tests passed,
but the actual user interaction path failed** because of QML semantics. Each rule
has a minimal reproducible bad/good from this codebase. The interaction test suite
(`tools/itest.sh`) and `tools/i18n-check.sh` enforce most of these.

---

## 1. Q_PROPERTY WRITE setters are NOT callable methods from QML

A property's `WRITE` function is exposed to QML **only** as the assignable property —
not as a callable method — unless it is also `Q_INVOKABLE` or a slot.

```cpp
// C++
Q_PROPERTY(int customerId READ customerId WRITE setCustomerId NOTIFY customerIdChanged)
void setCustomerId(int v);   // plain method — NOT Q_INVOKABLE, NOT a slot
```
```qml
// BAD — runtime "TypeError: Property 'setCustomerId' ... is not a function":
onActivated: (v) => invoiceEditor.setCustomerId(v)

// GOOD — assign the property; this invokes the WRITE accessor
// (which sets dirty + revalidates + emits the NOTIFY signal):
onActivated: (v) => invoiceEditor.customerId = v
```
**Rule:** prefer property assignment (`vm.prop = value`) in QML. Only call `vm.setX()`
if `setX` is explicitly `Q_INVOKABLE`/`slot`.

This bug shipped past visual QA and C++ probes because both drove the VM **directly
from C++** (where `setCustomerId(v)` works) and never executed the QML handler.

## 2. Qt 6 `required property` removes the implicit `model` in delegates

Declaring `required property` for roles is the modern read pattern, but it deletes
the implicit `model` context object — and required properties are **read-only**
projections, so there is no write-back through them.

```qml
// Delegate:
delegate: RowLayout {
    required property string qtyText        // read-only projection of the role
    ...
    // BAD — runtime "ReferenceError: model is not defined":
    FieldInput { onEditingFinished: model.qtyText = text }
}
```
```cpp
// GOOD — give the model an explicit mutation API:
Q_INVOKABLE void setCell(int row, const QString& field, const QString& value);
// (wraps setData() by field name)
```
```qml
// GOOD — call it with the delegate's index:
FieldInput { onEditingFinished: invoiceEditor.lines.setCell(lineRow.index, "qtyText", text) }
```
**Rule:** never rely on implicit `model.role` write-back in Qt 6 delegates. Use an
explicit invokable model API.

## 3. Never name a context property `locale`

Every QML `Item` has a built-in `locale` (`QLocale`) that **shadows** a same-named
context property inside Item scopes (Menu, ApplicationWindow, …), silently breaking
access (e.g. `locale.setLanguage(...)` → "is not a function").
```cpp
// GOOD:
engine.rootContext()->setContextProperty("i18n", &localeController);   // NOT "locale"
```
Avoid other Item property names for context props too (`state`, `parent`, `data`,
`opacity`, `enabled`, `width`, …).

## 4. Don't use imperative alignment under LayoutMirroring

`LayoutMirroring.enabled` (set app-wide for RTL) **swaps** explicit
`Text.AlignLeft`/`AlignRight`. Choosing alignment from `Theme.rtl` double-flips it.
```qml
// BAD — under mirroring this resolves to AlignLeft in Arabic, so a Latin name
// drifts away from its avatar while an Arabic name hugs it (ragged list):
horizontalAlignment: Theme.rtl ? Text.AlignRight : Text.AlignLeft

// GOOD — set the LOGICAL alignment; mirroring flips it to the inline-start in RTL,
// so all content anchors consistently regardless of script:
horizontalAlignment: Text.AlignLeft
```
Explicit `AlignLeft/Right` for genuinely directional content (money/number columns,
LTR identifiers) is allowed **with a justifying comment** (`i18n-check.sh` enforces this).

## 5. Controlled inputs — single mutation direction

Bind display from the VM; write back on user-completion events only — never
`onTextChanged` (it fires on the programmatic set too, causing loops).
```qml
// GOOD:
AppTextField {
    text: customerEditor.name                       // VM → field
    onEditingFinished: customerEditor.name = text   // field → VM (user event only)
}
```

## 6. No `count` on a raw model; expose explicit counts on the VM

`QAbstractItemModel`/`QAbstractListModel` have NO QML `count` property — `model.count`
silently yields `undefined`. Expose `Q_PROPERTY int totalCount/filteredCount` on the
per-screen view-model and drive UI state from those.

---

## Why these need an interaction test (not just screenshots / C++ tests)

Rules 1, 2, 5 fail **only when the QML handler runs** — a screenshot (which opens but
doesn't interact) and a C++ VM test (which bypasses QML) both pass. `tools/itest.sh`
drives the real handlers (`Select.activated`, `TextField.editingFinished`, model
invokables) via `QMetaObject::invokeMethod`/`QQmlExpression`, asserts VM state +
persistence, and fails on any captured QML runtime error. Run it before any PR that
touches editor/VM wiring.
