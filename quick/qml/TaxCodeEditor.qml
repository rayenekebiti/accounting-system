import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// TaxCodeEditor — authors a tax code (an append-only policy fact). Re-using a name records a new
// effective-dated VERSION; historical transactions keep the rate they captured. Bound to
// `taxCodeEditor` (TaxCodeEditorViewModel).
ModalSheet {
    id: root

    objectName: "taxCodeEditorRoot"
    title: qsTr("New Tax Code")

    property bool nameTouched: false
    property bool rateTouched: false
    property bool dateTouched: false
    property string saveError: ""

    function resetTouched() { nameTouched = false; rateTouched = false; dateTouched = false; saveError = "" }

    function fieldError(touched, key) {
        i18n.language   // dependency: retranslate the error on a live language switch
        if (key === "" || !(touched || taxCodeEditor.showErrors)) return ""
        switch (key) {
        case "required":    return qsTr("This field is required.")
        case "invalidRate": return qsTr("Enter a valid rate.")
        case "rangeRate":   return qsTr("Rate must be between 0 and 100.")
        case "invalidDate": return qsTr("Enter a date as YYYY-MM-DD.")
        default:            return ""
        }
    }

    readonly property bool zeroKind: taxCodeEditor.type === 3 || taxCodeEditor.type === 4  // Zero-rated / Exempt

    onOpened: { resetTouched(); nameField.forceActiveFocus() }
    onRequestClose: { if (taxCodeEditor.dirty) confirmDiscard.open(); else root.close() }

    function trySave() { taxCodeEditor.commit() }

    Connections {
        target: taxCodeEditor
        function onSaveFailed(msg) { root.saveError = msg }
    }

    Select {
        id: typeField
        objectName: "tx_type"
        Layout.fillWidth: true
        label:        qsTr("Tax type")
        model:        taxCodeEditor.typeOptions
        currentValue: taxCodeEditor.type
        onActivated: (v) => { taxCodeEditor.type = v }
    }

    AppTextField {
        id: nameField
        objectName: "tx_name"
        Layout.fillWidth: true
        label:       qsTr("Name")
        required:    true
        placeholder: qsTr("e.g. Standard Rate")
        text:        taxCodeEditor.name
        error:       root.fieldError(root.nameTouched, taxCodeEditor.nameError)
        onEditingFinished: { taxCodeEditor.name = text; root.nameTouched = true }
    }

    AppTextField {
        id: rateField
        objectName: "tx_rate"
        Layout.fillWidth: true
        visible:     !root.zeroKind
        label:       qsTr("Rate (%)")
        required:    true
        placeholder: qsTr("0.0")
        text:        taxCodeEditor.rate
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        error:       root.fieldError(root.rateTouched, taxCodeEditor.rateError)
        onEditingFinished: { taxCodeEditor.rate = text; root.rateTouched = true }
    }

    AppTextField {
        id: dateField
        objectName: "tx_date"
        Layout.fillWidth: true
        label:       qsTr("Effective from")
        required:    true
        placeholder: qsTr("YYYY-MM-DD")
        text:        taxCodeEditor.effectiveDate
        error:       root.fieldError(root.dateTouched, taxCodeEditor.dateError)
        onEditingFinished: { taxCodeEditor.effectiveDate = text; root.dateTouched = true }
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
        onConfirmed: { taxCodeEditor.discard(); root.close() }
        onCancelled: { }
    }
}
