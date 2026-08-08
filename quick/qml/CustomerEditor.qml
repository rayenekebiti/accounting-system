import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// CustomerEditor — ModalSheet-based customer create/edit form.
// Bound to the `customerEditor` context property (CustomerEditorViewModel).
//
// Keyboard workflow:
//   Open          → name field gets focus
//   Return        → advances name→email→phone→tax
//   Ctrl+S / Ctrl+Return → trySave()
//   Esc           → requestClose (guarded by dirty check)
ModalSheet {
    id: root

    objectName: "customerEditorRoot"

    title: customerEditor.isNew
           ? qsTr("New Customer")
           : qsTr("Edit %1").arg(customerEditor.name)

    // ── Touch tracking for progressive validation ────────────────────────────
    property bool nameTouched:  false
    property bool emailTouched: false
    property bool phoneTouched: false
    property bool taxTouched:   false

    // Inline save-error display
    property string saveError: ""

    function resetTouched() {
        nameTouched  = false
        emailTouched = false
        phoneTouched = false
        taxTouched   = false
        saveError    = ""
    }

    // Maps a VM error KEY → translated message. Empty until the field is touched
    // or a save attempt turned on showErrors. Reads i18n.language so the binding
    // re-evaluates live on language switch (qsTr alone wouldn't, since it's behind
    // a function call rather than lexically in the binding).
    function fieldError(touched, key) {
        const lang = i18n.language   // dependency: re-run on language change
        if (key === "" || !(touched || customerEditor.showErrors))
            return ""
        switch (key) {
        case "required":     return qsTr("This field is required.")
        case "tooLong":      return qsTr("This value is too long.")
        case "invalidEmail": return qsTr("Enter a valid email address.")
        default:             return ""
        }
    }

    // Focus name field on open; reset touched flags.
    onOpened: {
        resetTouched()
        nameField.forceActiveFocus()
    }

    // Dirty guard on close request.
    onRequestClose: {
        if (customerEditor.dirty)
            confirmDiscard.open()
        else
            root.close()
    }

    // ── Save / keyboard shortcuts ────────────────────────────────────────────
    function trySave() {
        customerEditor.commit()
        // On success:  customerEditor emits saved()  → Main closes + refreshes
        // On failure:  customerEditor emits validationFailed() → focus first bad field
    }

    Shortcut {
        sequences: ["Ctrl+S", "Ctrl+Return", "Ctrl+Enter"]
        context:   Qt.WindowShortcut
        onActivated: root.trySave()
    }

    // ── VM signal connections ────────────────────────────────────────────────
    Connections {
        target: customerEditor

        function onValidationFailed(field) {
            if      (field === "name")        nameField.forceActiveFocus()
            else if (field === "email")       emailField.forceActiveFocus()
            else if (field === "phone")       phoneField.forceActiveFocus()
            else if (field === "taxNumber")   taxField.forceActiveFocus()
        }

        function onSaveFailed(msg) {
            root.saveError = msg
        }
    }

    // ════════════════════════════════════════════════════════════════════════
    // Body content (goes into ModalSheet's default content alias → bodyCol)
    // ════════════════════════════════════════════════════════════════════════

    // ── Section: Details ─────────────────────────────────────────────────────
    Text {
        text:           qsTr("Details")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
    }

    AppTextField {
        id: nameField
        objectName: "ce_name"
        Layout.fillWidth: true
        label:    qsTr("Name")
        required: true
        text:     customerEditor.name
        error:    root.fieldError(root.nameTouched, customerEditor.nameError)
        onEditingFinished: {
            customerEditor.name = text   // property WRITE (setName is not a QML-callable method)
            root.nameTouched = true
        }
        Keys.onReturnPressed: emailField.forceActiveFocus()
    }

    AppTextField {
        id: emailField
        objectName: "ce_email"
        Layout.fillWidth: true
        label:            qsTr("Email")
        text:             customerEditor.email
        inputMethodHints: Qt.ImhEmailCharactersOnly
        error:            root.fieldError(root.emailTouched, customerEditor.emailError)
        onEditingFinished: {
            customerEditor.email = text
            root.emailTouched = true
        }
        Keys.onReturnPressed: phoneField.forceActiveFocus()
    }

    AppTextField {
        id: phoneField
        Layout.fillWidth: true
        label:            qsTr("Phone")
        text:             customerEditor.phone
        inputMethodHints: Qt.ImhDialableCharactersOnly
        error:            root.fieldError(root.phoneTouched, customerEditor.phoneError)
        onEditingFinished: {
            customerEditor.phone = text
            root.phoneTouched = true
        }
        Keys.onReturnPressed: taxField.forceActiveFocus()
    }

    AppTextField {
        id: taxField
        Layout.fillWidth: true
        label: qsTr("Tax number")
        text:  customerEditor.taxNumber
        error: root.fieldError(root.taxTouched, customerEditor.taxError)
        onEditingFinished: {
            customerEditor.taxNumber = text
            root.taxTouched = true
        }
    }

    Divider { Layout.fillWidth: true }

    // ── Balance (read-only derived) ───────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true

        Text {
            text:           qsTr("Balance")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family:    Theme.font.uiFamily
        }

        Item { Layout.fillWidth: true }

        CurrencyAmount {
            amount: customerEditor.balanceText
        }
    }

    Text {
        text:           qsTr("Calculated from invoices and payments")
        color:          Theme.color.textSecondary
        font.pixelSize: Theme.font.xs
        font.family:    Theme.font.uiFamily
        Layout.fillWidth: true
    }

    // Save error display (e.g. storage failure)
    Text {
        visible:          root.saveError.length > 0
        text:             root.saveError
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.sm
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }

    // ════════════════════════════════════════════════════════════════════════
    // Footer
    // ════════════════════════════════════════════════════════════════════════
    footerData: [
        Text {
            visible:        customerEditor.dirty
            text:           qsTr("Unsaved changes")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family:    Theme.font.uiFamily
        },
        Item { Layout.fillWidth: true },
        AppButton {
            // Deliver a statement (charges/payments/running balance) for this customer.
            visible: !customerEditor.isNew
            text:    qsTr("Statement")
            variant: "secondary"
            onClicked: { exportVm.exportCustomerStatement(customerEditor.editId); exportVm.openExportsFolder() }
        },
        AppButton {
            text:    qsTr("Cancel")
            variant: "ghost"
            onClicked: root.requestClose()
        },
        AppButton {
            text:    qsTr("Save")
            variant: "primary"
            onClicked: root.trySave()
        }
    ]

    // ── Discard confirm dialog ───────────────────────────────────────────────
    ConfirmDialog {
        id:             confirmDiscard
        title:          qsTr("Discard changes?")
        message:        qsTr("Your unsaved changes will be lost.")
        confirmText:    qsTr("Discard")
        cancelText:     qsTr("Keep editing")
        confirmVariant: "danger"

        onConfirmed: {
            customerEditor.discard()
            root.close()
        }
        onCancelled: { /* keep editor open */ }
    }
}
