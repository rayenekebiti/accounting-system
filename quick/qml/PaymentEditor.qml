import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// PaymentEditor — records a payment (customer + date + amount). Bound to `paymentEditor`
// (PaymentEditorViewModel). On save, Main opens the AllocationEditor for the new payment.
// (No reference/notes field — PaymentRecorded has no such column; see docs/payments-workflow.md.)
ModalSheet {
    id: root

    objectName: "paymentEditorRoot"
    title: qsTr("Record Payment")

    property bool customerTouched: false
    property bool amountTouched:   false
    property bool dateTouched:     false
    property string saveError: ""

    function resetTouched() { customerTouched = false; amountTouched = false; dateTouched = false; saveError = "" }

    function fieldError(touched, key) {
        const lang = i18n.language
        if (key === "" || !(touched || paymentEditor.showErrors)) return ""
        switch (key) {
        case "required":       return qsTr("This field is required.")
        case "invalidAmount":  return qsTr("Enter a valid amount.")
        case "positiveAmount": return qsTr("Amount must be greater than zero.")
        case "amountTooLarge": return qsTr("Amount is too large.")
        case "invalidDate":    return qsTr("Enter a date as YYYY-MM-DD.")
        default:               return ""
        }
    }

    onOpened: { resetTouched(); customerField.forceActiveFocus() }
    onRequestClose: { if (paymentEditor.dirty) confirmDiscard.open(); else root.close() }

    function trySave() { paymentEditor.commit() }

    Shortcut {
        sequences: ["Ctrl+S", "Ctrl+Return", "Ctrl+Enter"]
        context:   Qt.WindowShortcut
        onActivated: root.trySave()
    }

    Connections {
        target: paymentEditor
        function onValidationFailed(field) {
            if      (field === "customer") customerField.forceActiveFocus()
            else if (field === "amount")   amountField.forceActiveFocus()
            else if (field === "date")     dateField.forceActiveFocus()
        }
        function onSaveFailed(msg) { root.saveError = msg }
    }

    Text {
        text:           qsTr("Payment")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
    }

    Select {
        id: customerField
        objectName: "pe_customer"
        Layout.fillWidth: true
        label:        qsTr("Customer")
        required:     true
        model:        paymentEditor.customerOptions
        currentValue: paymentEditor.customerId
        error:        root.fieldError(root.customerTouched, paymentEditor.customerError)
        onActivated: (v) => { paymentEditor.customerId = v; root.customerTouched = true }
    }

    AppTextField {
        id: dateField
        objectName: "pe_date"
        Layout.fillWidth: true
        label:       qsTr("Payment date")
        required:    true
        placeholder: qsTr("YYYY-MM-DD")
        text:        paymentEditor.date
        error:       root.fieldError(root.dateTouched, paymentEditor.dateError)
        onEditingFinished: { paymentEditor.date = text; root.dateTouched = true }
        Keys.onReturnPressed: amountField.forceActiveFocus()
    }

    AppTextField {
        id: amountField
        objectName: "pe_amount"
        Layout.fillWidth: true
        label:            qsTr("Amount")
        required:         true
        placeholder:      qsTr("0.00")
        text:             paymentEditor.amount
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        error:            root.fieldError(root.amountTouched, paymentEditor.amountError)
        onEditingFinished: { paymentEditor.amount = text; root.amountTouched = true }
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

    footerData: [
        Text {
            visible:        paymentEditor.dirty
            text:           qsTr("Unsaved changes")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family:    Theme.font.uiFamily
        },
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Cancel"); variant: "ghost";   onClicked: root.requestClose() },
        AppButton { text: qsTr("Save");   variant: "primary"; onClicked: root.trySave() }
    ]

    ConfirmDialog {
        id:             confirmDiscard
        title:          qsTr("Discard changes?")
        message:        qsTr("Your unsaved changes will be lost.")
        confirmText:    qsTr("Discard")
        cancelText:     qsTr("Keep editing")
        confirmVariant: "danger"
        onConfirmed: { paymentEditor.discard(); root.close() }
        onCancelled: { }
    }
}
