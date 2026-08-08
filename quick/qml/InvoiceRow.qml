import QtQuick
import QtQuick.Layouts
import App

ListRowCard {
    id: root

    width: ListView.view ? ListView.view.width : 0

    signal rowClicked()
    onClicked: rowClicked()

    RowLayout {
        width: parent.width
        spacing: Theme.space.md

        // Left: invoice number + customer name
        ColumnLayout {
            spacing: Theme.space.xxs

            Text {
                text:  model.number
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.md
                font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily
            }

            Text {
                text:  model.customer
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.sm
                font.family: Theme.font.uiFamily
            }
        }

        Item { Layout.fillWidth: true }

        // Right: amount + derived outstanding (from the settlement engine)
        ColumnLayout {
            spacing: Theme.space.xxs
            CurrencyAmount {
                amount: model.totalText
                Layout.alignment: Qt.AlignRight
            }
            Text {
                visible: model.paymentStatus !== "Unpaid"
                text:    qsTr("Outstanding %1").arg(model.outstandingText)
                color:   Theme.color.textSecondary
                font.pixelSize: Theme.font.xs
                font.family:    Theme.font.uiFamily
                Layout.alignment: Qt.AlignRight
            }
        }

        // Settlement badge (only when there is payment activity) + lifecycle status.
        Badge {
            visible: model.paymentStatus !== "Unpaid"
            tone:    model.paymentStatus === "Paid" ? "" : "pending"
            text:    model.paymentStatus === "Paid" ? qsTr("Paid") : qsTr("Partial")
        }

        StatusBadge {
            status: model.status
        }
    }
}
