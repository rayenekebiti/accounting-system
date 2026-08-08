import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// AllocationEditor — apply a payment to invoices (partial allowed) + review/reverse existing
// allocations. Bound to `paymentAllocation` (PaymentAllocationViewModel). Every action is an
// authoritative settlement event; outstanding is derived (never mutated here).
ModalSheet {
    id: root

    objectName: "allocationEditorRoot"
    title: qsTr("Allocate payment")

    property string actionError: ""
    property int    pendingReverseId: -1

    onOpened: { root.actionError = "" }
    onRequestClose: root.close()

    Connections {
        target: paymentAllocation
        function onActionFailed(msg) {
            root.actionError = (msg === "positiveAmount")
                ? qsTr("Enter an amount greater than zero.")
                : msg
        }
        function onChanged() { root.actionError = "" }
    }

    // ── Header banner: customer / amount / unallocated ────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xxs
            Text {
                text:  paymentAllocation.customerName
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.md
                font.weight:    Theme.font.weightBold
                font.family:    Theme.font.uiFamily
            }
            Text {
                text:  qsTr("Payment %1").arg(paymentAllocation.paymentAmountText)
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.sm
                font.family:    Theme.font.uiFamily
            }
        }
        ColumnLayout {
            spacing: Theme.space.xxs
            Text {
                text:  qsTr("Unallocated")
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs
                font.family:    Theme.font.uiFamily
            }
            CurrencyAmount { amount: paymentAllocation.unallocatedText }
        }
    }

    Divider { Layout.fillWidth: true }

    // ── Apply to invoices ──────────────────────────────────────────────────────
    Text {
        text:           qsTr("Apply to invoices")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
    }

    Text {
        visible:        paymentAllocation.allocatableInvoices.length === 0
        text:           qsTr("No open invoices for this customer.")
        color:          Theme.color.textSecondary
        font.pixelSize: Theme.font.sm
        font.family:    Theme.font.uiFamily
        Layout.fillWidth: true
    }

    Repeater {
        model: paymentAllocation.allocatableInvoices
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: Theme.space.sm

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.xxs
                Text {
                    text:  modelData.number
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.base
                    font.family:    Theme.font.uiFamily
                    horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                }
                Text {
                    text:  qsTr("Outstanding %1").arg(modelData.outstandingText)
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.xs
                    font.family:    Theme.font.uiFamily
                    horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                }
            }

            AppTextField {
                id: allocField
                Layout.preferredWidth: 120
                placeholder:      qsTr("0.00")
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            }

            AppButton {
                text:    qsTr("Allocate")
                variant: "primary"
                enabled: allocField.text.length > 0
                onClicked: {
                    if (paymentAllocation.allocate(modelData.invoiceId, allocField.text))
                        allocField.text = ""
                }
            }
        }
    }

    Divider { Layout.fillWidth: true }

    // ── Existing allocations (lineage) ─────────────────────────────────────────
    Text {
        text:           qsTr("Allocations")
        font.pixelSize: Theme.font.sm
        font.weight:    Theme.font.weightSemibold
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        Layout.fillWidth: true
    }

    Text {
        visible:        paymentAllocation.existingAllocations.length === 0
        text:           qsTr("Nothing allocated yet.")
        color:          Theme.color.textSecondary
        font.pixelSize: Theme.font.sm
        font.family:    Theme.font.uiFamily
        Layout.fillWidth: true
    }

    Repeater {
        model: paymentAllocation.existingAllocations
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Text {
                text:  modelData.invoiceNumber
                color: modelData.reversed ? Theme.color.textSecondary : Theme.color.textPrimary
                font.pixelSize: Theme.font.base
                font.family:    Theme.font.uiFamily
                font.strikeout: modelData.reversed
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            }
            CurrencyAmount { amount: modelData.amountText }
            Badge {
                visible: modelData.reversed
                tone:    "pending"
                text:    qsTr("Reversed")
            }
            AppButton {
                visible: !modelData.reversed
                text:    qsTr("Reverse")
                variant: "ghost"
                onClicked: { root.pendingReverseId = modelData.allocationId; confirmReverse.open() }
            }
        }
    }

    Text {
        visible:          root.actionError.length > 0
        text:             root.actionError
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.sm
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }

    footerData: [
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Done"); variant: "primary"; onClicked: root.close() }
    ]

    ConfirmDialog {
        id:             confirmReverse
        title:          qsTr("Reverse allocation?")
        message:        qsTr("This appends a reversal (settlement history is never deleted). Outstanding will be restored.")
        confirmText:    qsTr("Reverse")
        cancelText:     qsTr("Cancel")
        confirmVariant: "danger"
        onConfirmed: paymentAllocation.reverse(root.pendingReverseId)
        onCancelled: { }
    }
}
