import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// SupplierEditor — ModalSheet-based supplier create/edit form. Mirrors CustomerEditor.
// Bound to the `supplierEditor` context property (SupplierEditorViewModel).
ModalSheet {
    id: root

    objectName: "supplierEditorRoot"

    title: supplierEditor.isNew
           ? qsTr("New Supplier")
           : qsTr("Edit %1").arg(supplierEditor.name)

    property bool nameTouched:  false
    property bool emailTouched: false
    property bool phoneTouched: false
    property bool taxTouched:   false
    property string saveError: ""

    function resetTouched() {
        nameTouched = false; emailTouched = false; phoneTouched = false; taxTouched = false
        saveError = ""
    }

    function fieldError(touched, key) {
        const lang = i18n.language   // dependency: re-run on language change
        if (key === "" || !(touched || supplierEditor.showErrors))
            return ""
        switch (key) {
        case "required":     return qsTr("This field is required.")
        case "tooLong":      return qsTr("This value is too long.")
        case "invalidEmail": return qsTr("Enter a valid email address.")
        default:             return ""
        }
    }

    onOpened: {
        resetTouched()
        nameField.forceActiveFocus()
    }

    onRequestClose: {
        if (supplierEditor.dirty)
            confirmDiscard.open()
        else
            root.close()
    }

    function trySave() {
        supplierEditor.commit()
    }

    Shortcut {
        sequences: ["Ctrl+S", "Ctrl+Return", "Ctrl+Enter"]
        context:   Qt.WindowShortcut
        onActivated: root.trySave()
    }

    Connections {
        target: supplierEditor

        function onValidationFailed(field) {
            if      (field === "name")      nameField.forceActiveFocus()
            else if (field === "email")     emailField.forceActiveFocus()
            else if (field === "phone")     phoneField.forceActiveFocus()
            else if (field === "taxNumber") taxField.forceActiveFocus()
        }
        function onSaveFailed(msg) { root.saveError = msg }
    }

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
        objectName: "se_name"
        Layout.fillWidth: true
        label:    qsTr("Name")
        required: true
        text:     supplierEditor.name
        error:    root.fieldError(root.nameTouched, supplierEditor.nameError)
        onEditingFinished: { supplierEditor.name = text; root.nameTouched = true }
        Keys.onReturnPressed: emailField.forceActiveFocus()
    }

    AppTextField {
        id: emailField
        objectName: "se_email"
        Layout.fillWidth: true
        label:            qsTr("Email")
        text:             supplierEditor.email
        inputMethodHints: Qt.ImhEmailCharactersOnly
        error:            root.fieldError(root.emailTouched, supplierEditor.emailError)
        onEditingFinished: { supplierEditor.email = text; root.emailTouched = true }
        Keys.onReturnPressed: phoneField.forceActiveFocus()
    }

    AppTextField {
        id: phoneField
        Layout.fillWidth: true
        label:            qsTr("Phone")
        text:             supplierEditor.phone
        inputMethodHints: Qt.ImhDialableCharactersOnly
        error:            root.fieldError(root.phoneTouched, supplierEditor.phoneError)
        onEditingFinished: { supplierEditor.phone = text; root.phoneTouched = true }
        Keys.onReturnPressed: taxField.forceActiveFocus()
    }

    AppTextField {
        id: taxField
        Layout.fillWidth: true
        label: qsTr("Tax number")
        text:  supplierEditor.taxNumber
        error: root.fieldError(root.taxTouched, supplierEditor.taxError)
        onEditingFinished: { supplierEditor.taxNumber = text; root.taxTouched = true }
    }

    Divider { Layout.fillWidth: true }

    // ── Balance (read-only payable) ───────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true

        Text {
            text:           qsTr("Payable balance")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family:    Theme.font.uiFamily
        }
        Item { Layout.fillWidth: true }
        CurrencyAmount { amount: supplierEditor.balanceText }
    }

    Text {
        text:           qsTr("Amount currently owed to this supplier")
        color:          Theme.color.textSecondary
        font.pixelSize: Theme.font.xs
        font.family:    Theme.font.uiFamily
        Layout.fillWidth: true
    }

    Text {
        visible:          root.saveError.length > 0
        text:             root.saveError
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.sm
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }

    // ── Footer ────────────────────────────────────────────────────────────────
    footerData: [
        Text {
            visible:        supplierEditor.dirty
            text:           qsTr("Unsaved changes")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family:    Theme.font.uiFamily
        },
        Item { Layout.fillWidth: true },
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

    ConfirmDialog {
        id:             confirmDiscard
        title:          qsTr("Discard changes?")
        message:        qsTr("Your unsaved changes will be lost.")
        confirmText:    qsTr("Discard")
        cancelText:     qsTr("Keep editing")
        confirmVariant: "danger"
        onConfirmed: { supplierEditor.discard(); root.close() }
        onCancelled: { /* keep editor open */ }
    }
}
