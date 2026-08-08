import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// PaymentsScreen — lists payments from the settlement engine. Status is derived
// (Unallocated / Partial / Allocated). Bound to `paymentsVm` (PaymentsViewModel).
Item {
    id: root

    signal newPaymentRequested()
    signal rowActivated(int paymentId)

    property string activeCategory: "All"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Payments")
            subtitle: qsTr("%n payment(s)", "", paymentsVm.totalCount)

            AppButton {
                text:    qsTr("New Payment")
                variant: "primary"
                onClicked: root.newPaymentRequested()
            }
        }

        SummaryBar {
            objectName: "paymentSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "payHeroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Payments")
                value: String(paymentsVm.totalPayments)
                sub:   qsTr("%n unallocated", "", paymentsVm.unallocatedCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Received")
                value: paymentsVm.totalReceivedText
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Unallocated")
                value: paymentsVm.unallocatedText
                tone:  paymentsVm.unallocatedCount > 0 ? "pending" : ""
                sub:   qsTr("credit")
            }
        }

        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search payments…")
            onSearchTextChanged: paymentsVm.setSearchText(searchText)

            Chip { text: qsTr("All");         selected: root.activeCategory === "All";         onClicked: { root.activeCategory = "All";         paymentsVm.setCategoryFilter("All") } }
            Chip { text: qsTr("Unallocated"); selected: root.activeCategory === "Unallocated"; onClicked: { root.activeCategory = "Unallocated"; paymentsVm.setCategoryFilter("Unallocated") } }
            Chip { text: qsTr("Partial");     selected: root.activeCategory === "Partial";     onClicked: { root.activeCategory = "Partial";     paymentsVm.setCategoryFilter("Partial") } }
            Chip { text: qsTr("Allocated");   selected: root.activeCategory === "Allocated";   onClicked: { root.activeCategory = "Allocated";   paymentsVm.setCategoryFilter("Allocated") } }
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: paymentsVm.totalCount === 0
                icon:        "💵"
                title:       qsTr("No payments yet")
                description: qsTr("Record a payment from a customer, then allocate it to invoices.")
                actionText:  qsTr("New Payment")
                onActionClicked: root.newPaymentRequested()
            }

            EmptyState {
                anchors.fill: parent
                visible: paymentsVm.totalCount > 0 && paymentsVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            ListView {
                objectName: "paymentList"
                anchors.fill: parent
                visible: paymentsVm.totalCount > 0 && paymentsVm.filteredCount > 0
                model:   paymentsVm.listModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: root.rowActivated(model.paymentId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.xxs

                            Text {
                                text:            model.customer
                                color:           Theme.color.textPrimary
                                font.pixelSize:  Theme.font.md
                                font.weight:     Theme.font.weightBold
                                font.family:     Theme.font.uiFamily
                                elide:           Text.ElideRight
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                            Text {
                                text:            model.date
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                        }

                        Badge {
                            tone: model.status === "Allocated" ? "" : "pending"
                            text: model.status === "Allocated" ? qsTr("Allocated")
                                : model.status === "Partial"   ? qsTr("Partial")
                                : qsTr("Unallocated")
                        }

                        CurrencyAmount {
                            amount: model.amountText
                        }
                    }
                }
            }
        }
    }
}
