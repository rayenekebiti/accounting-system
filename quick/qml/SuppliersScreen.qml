import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// SuppliersScreen — mirrors CustomersScreen. Suppliers are payables (no at-risk/overdue).
// Bound to the `suppliersVm` context property (SuppliersViewModel).
Item {
    id: root

    signal newSupplierRequested()
    signal rowActivated(int supplierId)

    property string activeCategory: "All"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        // ── PageHeader ────────────────────────────────────────────────────────
        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Suppliers")
            subtitle: qsTr("%n supplier(s)", "", suppliersVm.totalCount)

            AppButton {
                text:    qsTr("New Supplier")
                variant: "primary"
                onClicked: root.newSupplierRequested()
            }
        }

        // ── Summary bar ───────────────────────────────────────────────────────
        SummaryBar {
            objectName: "supplierSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "supHeroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Suppliers")
                value: String(suppliersVm.totalSuppliers)
                sub:   qsTr("%n owing", "", suppliersVm.owingCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Payable")
                value: suppliersVm.outstandingText
                sub:   qsTr("%n with balance", "", suppliersVm.withBalanceCount)
            }
        }

        // ── FilterBar ─────────────────────────────────────────────────────────
        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search suppliers…")
            onSearchTextChanged: suppliersVm.setSearchText(searchText)

            Chip { text: qsTr("All");   selected: root.activeCategory === "All";   onClicked: { root.activeCategory = "All";   suppliersVm.setCategoryFilter("All")   } }
            Chip { text: qsTr("Owing"); selected: root.activeCategory === "Owing"; onClicked: { root.activeCategory = "Owing"; suppliersVm.setCategoryFilter("Owing") } }
        }

        // ── List area (state machine) ──────────────────────────────────────────
        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: suppliersVm.totalCount === 0
                icon:        "🏢"
                title:       qsTr("No suppliers yet")
                description: qsTr("Add your first supplier to start tracking who you owe.")
                actionText:  qsTr("New Supplier")
                onActionClicked: root.newSupplierRequested()
            }

            EmptyState {
                anchors.fill: parent
                visible: suppliersVm.totalCount > 0 && suppliersVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            ListView {
                objectName: "supplierList"
                anchors.fill: parent
                visible: suppliersVm.totalCount > 0 && suppliersVm.filteredCount > 0
                model:   suppliersVm.listModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: root.rowActivated(model.supplierId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        Avatar {
                            name:     model.name
                            diameter: 32
                        }

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
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
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
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
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
