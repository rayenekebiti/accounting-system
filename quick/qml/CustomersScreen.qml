import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

Item {
    id: root

    signal newCustomerRequested()
    signal rowActivated(int customerId)

    // Active category chip
    property string activeCategory: "All"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        // ── PageHeader ────────────────────────────────────────────────────────
        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Customers")
            subtitle: qsTr("%n customer(s)", "", customersVm.totalCount)

            AppButton {
                text:    qsTr("Outstanding balances")
                variant: "secondary"
                onClicked: { exportVm.exportOutstandingSummary(); exportVm.openExportsFolder() }
            }
            AppButton {
                text:    qsTr("New Customer")
                variant: "primary"
                onClicked: root.newCustomerRequested()
            }
        }

        // ── Summary bar ───────────────────────────────────────────────────────
        SummaryBar {
            objectName: "customerSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "custHeroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Customers")
                value: String(customersVm.totalCustomers)
                sub:   qsTr("%n owing", "", customersVm.owingCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Outstanding")
                value: customersVm.outstandingText
                sub:   qsTr("%n with balance", "", customersVm.withBalanceCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("At-risk")
                value: String(customersVm.atRiskCount)
                tone:  customersVm.atRiskCount > 0 ? "pending" : ""
                sub:   qsTr("overdue")
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Paid")
                value: customersVm.totalPaidText
                sub:   qsTr("credit %1").arg(customersVm.creditText)
            }
        }

        // ── FilterBar ─────────────────────────────────────────────────────────
        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search customers…")
            onSearchTextChanged: customersVm.setSearchText(searchText)

            Chip { text: qsTr("All");     selected: root.activeCategory === "All";    onClicked: { root.activeCategory = "All";    customersVm.setCategoryFilter("All")    } }
            Chip { text: qsTr("Owing");   selected: root.activeCategory === "Owing";  onClicked: { root.activeCategory = "Owing";  customersVm.setCategoryFilter("Owing")  } }
            Chip { text: qsTr("At-risk"); selected: root.activeCategory === "AtRisk"; onClicked: { root.activeCategory = "AtRisk"; customersVm.setCategoryFilter("AtRisk") } }
        }

        // ── List area (state machine) ──────────────────────────────────────────
        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            // Empty (no customers at all)
            EmptyState {
                anchors.fill: parent
                visible: customersVm.totalCount === 0
                icon:        "👤"
                title:       qsTr("No customers yet")
                description: qsTr("Add your first customer to start tracking who owes you.")
                actionText:  qsTr("New Customer")
                onActionClicked: root.newCustomerRequested()
            }

            // Filtered-empty (search / category chip yields nothing)
            EmptyState {
                anchors.fill: parent
                visible: customersVm.totalCount > 0 && customersVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            // Populated list
            ListView {
                objectName: "customerList"
                anchors.fill: parent
                visible: customersVm.totalCount > 0 && customersVm.filteredCount > 0
                model:   customersVm.listModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: root.rowActivated(model.customerId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        Avatar {
                            name:     model.name
                            diameter: 32
                        }

                        // Identity column takes the flex space so long names AND
                        // long emails elide within it instead of pushing the row
                        // wide (dataset pressure-test: long/multilingual values).
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.xxs

                            Text {
                                text:            model.name
                                color:           Theme.color.textPrimary
                                font.pixelSize:  Theme.font.md
                                font.weight:     Theme.font.weightBold
                                font.family:     Theme.font.uiFamily
                                elide:           Text.ElideRight
                                // Anchor to the inline-start regardless of script,
                                // so Latin and Arabic names both hug the avatar
                                // (not content-based: Latin would drift left in RTL).
                                horizontalAlignment: Text.AlignLeft  // LayoutMirroring flips to inline-start (right in RTL)
                                Layout.fillWidth: true
                            }

                            Text {
                                text: model.email.length > 0
                                      ? model.email
                                      : (model.phone.length > 0 ? model.phone : "—")
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                elide:           Text.ElideRight
                                horizontalAlignment: Text.AlignLeft  // LayoutMirroring flips to inline-start (right in RTL)
                                Layout.fillWidth: true
                            }
                        }

                        Badge {
                            visible: model.atRisk
                            tone:    "pending"
                            text:    qsTr("Overdue")
                        }

                        CurrencyAmount {
                            amount: model.balanceText
                        }
                    }
                }
            }
        }
    }
}
