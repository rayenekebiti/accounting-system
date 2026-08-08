import QtQuick
import QtQuick.Controls.Basic
import App

// FieldInput — bare token-styled TextField.
// Reused by AppTextField (with label+error wrapping) and line-editor cells.
// Inherits TextField's editingFinished and accepted signals directly.
TextField {
    id: root

    property bool hasError: false

    // A screen reader must be able to identify the field. TextField exposes its VALUE but not a
    // NAME, so default the accessible name to the placeholder; wrappers (AppTextField) and cell
    // editors override this with their visible label. Without it the field is announced "unnamed".
    Accessible.name: root.placeholderText

    font.family:    Theme.font.uiFamily
    font.pixelSize: Theme.font.base
    // Token colors — without these the Basic style renders entered text in a
    // low-contrast default gray (readability/WCAG issue on every input field).
    color:                Theme.color.textPrimary
    placeholderTextColor: Theme.color.textSecondary
    selectionColor:       Theme.color.brand
    selectedTextColor:    Theme.color.textOnBrand

    leftPadding:   Theme.space.md
    rightPadding:  Theme.space.md
    topPadding:    Theme.space.sm
    bottomPadding: Theme.space.sm

    background: Rectangle {
        radius:       Theme.radius.md
        color:        Theme.color.surface
        border.color: root.hasError
                          ? Theme.color.expense
                          : (root.activeFocus ? Theme.color.focusRing : Theme.color.border)
        border.width: root.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
    }
}
