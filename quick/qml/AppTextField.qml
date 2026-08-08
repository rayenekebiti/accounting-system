import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// AppTextField — label + FieldInput + error text.
// For bare input without wrapper, use FieldInput directly.
ColumnLayout {
    id: root

    property string label:               ""
    property string placeholder:         ""
    property string error:               ""
    property bool   required:            false
    property alias  text:                field.text
    property alias  horizontalAlignment: field.horizontalAlignment
    property alias  inputMethodHints:    field.inputMethodHints
    property alias  validator:           field.validator

    signal editingFinished()

    spacing: Theme.space.xs

    // ── Label row ────────────────────────────────────────────────────────────
    Text {
        visible: root.label.length > 0
        font.pixelSize: Theme.font.sm
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary

        text: root.required
              ? root.label + " <font color='" + Theme.color.expense + "'>*</font>"
              : root.label

        textFormat: Text.RichText
    }

    // ── Input ────────────────────────────────────────────────────────────────
    FieldInput {
        id: field
        Layout.fillWidth: true
        placeholderText:  root.placeholder
        // Announce the visible label (falling back to the placeholder) so the field is named.
        Accessible.name:  root.label.length > 0 ? root.label : root.placeholder
        hasError:         root.error.length > 0
        onEditingFinished: root.editingFinished()
    }

    // ── Error text ───────────────────────────────────────────────────────────
    Text {
        visible:        root.error.length > 0
        text:           root.error
        color:          Theme.color.expense
        font.pixelSize: Theme.font.xs
        font.family:    Theme.font.uiFamily
        wrapMode:       Text.WordWrap
        Layout.fillWidth: true
    }
}
