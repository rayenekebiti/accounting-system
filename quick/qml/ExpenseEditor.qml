import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// ExpenseEditor — records or corrects an expense (supplier · date · amount · category ·
// payment method · memo). Bound to `expenseEditor` (ExpenseEditorViewModel). On save the
// expense + its balanced ledger posting are authored as ONE atomic authoritative fact.
// On edit the payment method is fixed (the delta posting keeps the same credit account).
ModalSheet {
    id: root

    objectName: "expenseEditorRoot"
    title: expenseEditor.editingId >= 0 ? qsTr("Edit Expense") : qsTr("Record Expense")

    property bool amountTouched: false
    property bool dateTouched:   false
    property string saveError: ""

    function resetTouched() { amountTouched = false; dateTouched = false; saveError = "" }

    function fieldError(touched, key) {
        i18n.language   // dependency: retranslate the error on a live language switch
        if (key === "" || !(touched || expenseEditor.showErrors)) return ""
        switch (key) {
        case "required":       return qsTr("This field is required.")
        case "invalidAmount":  return qsTr("Enter a valid amount.")
        case "positiveAmount": return qsTr("Amount must be greater than zero.")
        case "amountTooLarge": return qsTr("Amount is too large.")
        case "invalidDate":    return qsTr("Enter a date as YYYY-MM-DD.")
        default:               return ""
        }
    }

    onOpened: { resetTouched(); supplierField.forceActiveFocus() }
    onRequestClose: { if (expenseEditor.dirty) confirmDiscard.open(); else root.close() }

    function trySave() { expenseEditor.commit() }

    Shortcut {
        sequences: ["Ctrl+S", "Ctrl+Return", "Ctrl+Enter"]
        context:   Qt.WindowShortcut
        onActivated: root.trySave()
    }

    Connections {
        target: expenseEditor
        function onValidationFailed(field) {
            if      (field === "amount") amountField.forceActiveFocus()
            else if (field === "date")   dateField.forceActiveFocus()
        }
        function onSaveFailed(msg) { root.saveError = msg }
    }

    Select {
        id: supplierField
        objectName: "ex_supplier"
        Layout.fillWidth: true
        label:        qsTr("Supplier")
        model:        expenseEditor.supplierOptions
        currentValue: expenseEditor.supplierId
        onActivated: (v) => { expenseEditor.supplierId = v }
    }

    AppTextField {
        id: dateField
        objectName: "ex_date"
        Layout.fillWidth: true
        label:       qsTr("Expense date")
        required:    true
        placeholder: qsTr("YYYY-MM-DD")
        text:        expenseEditor.date
        error:       root.fieldError(root.dateTouched, expenseEditor.dateError)
        onEditingFinished: { expenseEditor.date = text; root.dateTouched = true }
        Keys.onReturnPressed: amountField.forceActiveFocus()
    }

    AppTextField {
        id: amountField
        objectName: "ex_amount"
        Layout.fillWidth: true
        label:            qsTr("Amount")
        required:         true
        placeholder:      qsTr("0.00")
        text:             expenseEditor.amount
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        error:            root.fieldError(root.amountTouched, expenseEditor.amountError)
        onEditingFinished: { expenseEditor.amount = text; root.amountTouched = true }
    }

    Select {
        id: categoryField
        objectName: "ex_category"
        Layout.fillWidth: true
        label:        qsTr("Category")
        model:        expenseEditor.categoryOptions
        currentValue: expenseEditor.category
        onActivated: (v) => { expenseEditor.category = v }
    }

    // Payment method — a two-option segmented toggle. Fixed on edit (a correction keeps the
    // same credit account so the delta posting stays balanced against Cash / Accounts Payable).
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space.xxs
        Text {
            text:           qsTr("Payment method")
            font.pixelSize: Theme.font.sm
            font.weight:    Theme.font.weightSemibold
            font.family:    Theme.font.uiFamily
            color:          Theme.color.textSecondary
        }
        RowLayout {
            spacing: Theme.space.sm
            Chip {
                text: qsTr("Cash"); selected: expenseEditor.paymentMethod === 0
                enabled: expenseEditor.editingId < 0
                onClicked: expenseEditor.paymentMethod = 0
            }
            Chip {
                text: qsTr("Credit (payable)"); selected: expenseEditor.paymentMethod === 1
                enabled: expenseEditor.editingId < 0
                onClicked: expenseEditor.paymentMethod = 1
            }
            Item { Layout.fillWidth: true }
        }
        Text {
            visible:        expenseEditor.editingId >= 0
            text:           qsTr("Payment method can't change on a correction.")
            color:          Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family:    Theme.font.uiFamily
        }
    }

    Select {
        id: taxField
        objectName: "ex_tax"
        Layout.fillWidth: true
        label:        qsTr("Tax code")
        model:        expenseEditor.taxCodeOptions
        currentValue: expenseEditor.taxCode
        onActivated: (v) => { expenseEditor.taxCode = v }
    }

    // Tax summary — net (amount) + recoverable input tax = total paid. Engine-consistent.
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md
        Text {
            text:  qsTr("Tax %1 · Total %2").arg(expenseEditor.taxText).arg(expenseEditor.totalText)
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm
            font.family:    Theme.font.uiFamily
            horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            Layout.fillWidth: true
        }
    }

    AppTextField {
        id: memoField
        objectName: "ex_memo"
        Layout.fillWidth: true
        label:       qsTr("Memo")
        placeholder: qsTr("Optional note")
        text:        expenseEditor.memo
        onEditingFinished: expenseEditor.memo = text
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
            visible:        expenseEditor.dirty
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
        onConfirmed: { expenseEditor.discard(); root.close() }
        onCancelled: { }
    }
}
