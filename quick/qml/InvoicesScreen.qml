import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

Item {
    id: root

    signal newInvoiceRequested()
    signal rowActivated(int invoiceId)
    signal viewLedgerRequested()

    // Active status chip
    property string activeStatus: "All"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        // ── PageHeader ────────────────────────────────────────────────────────
        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Invoices")
            subtitle: qsTr("%n invoice(s)", "", invoicesVm.totalCount) + " · " + qsTr("%n awaiting payment", "", invoicesVm.awaitingCount)

            AppButton {
                text:    qsTr("New Invoice")
                variant: "primary"
                onClicked: root.newInvoiceRequested()
            }
        }

        // ── Summary bar ───────────────────────────────────────────────────────
        // One grouped surface. "Outstanding" leads as the emphasized hero; the
        // pipeline then reads inline-start→end (Outstanding ⊃ Overdue → Paid →
        // Draft) through order + colour — compact, start-aligned, tabular.
        SummaryBar {
            objectName: "summaryCard"
            Layout.fillWidth: true

            MetricCell {
                objectName: "heroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Outstanding")
                value: invoicesVm.outstandingText
                sub:   qsTr("%n unpaid", "", invoicesVm.outstandingCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Overdue")
                value: invoicesVm.overdueText
                tone:  invoicesVm.overdueCount > 0 ? "pending" : ""
                sub:   qsTr("%n invoice(s)", "", invoicesVm.overdueCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Paid")
                value: invoicesVm.paidText
                tone:  "income"
                sub:   qsTr("%n invoice(s)", "", invoicesVm.paidCount)
            }

            Divider { orientation: "vertical"; Layout.fillHeight: true }

            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Draft")
                value: String(invoicesVm.draftCount)
                sub:   qsTr("not sent")
            }
        }

        // ── FilterBar ─────────────────────────────────────────────────────────
        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search invoices…")
            onSearchTextChanged: invoicesVm.setSearchText(searchText)

            Chip { text: qsTr("All");     selected: root.activeStatus === "All";     onClicked: { root.activeStatus = "All";     invoicesVm.setStatusFilter("All")     } }
            Chip { text: qsTr("Draft");   selected: root.activeStatus === "Draft";   onClicked: { root.activeStatus = "Draft";   invoicesVm.setStatusFilter("Draft")   } }
            Chip { text: qsTr("Posted");  selected: root.activeStatus === "Posted";  onClicked: { root.activeStatus = "Posted";  invoicesVm.setStatusFilter("Posted")  } }
            Chip { text: qsTr("Paid");    selected: root.activeStatus === "Paid";    onClicked: { root.activeStatus = "Paid";    invoicesVm.setStatusFilter("Paid")    } }
            Chip { text: qsTr("Overdue"); selected: root.activeStatus === "Overdue"; onClicked: { root.activeStatus = "Overdue"; invoicesVm.setStatusFilter("Overdue") } }
            Chip { text: qsTr("Void");    selected: root.activeStatus === "Void";    onClicked: { root.activeStatus = "Void";    invoicesVm.setStatusFilter("Void")    } }
        }

        // ── List area (state machine) ──────────────────────────────────────────
        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            // Loading state (placeholder — data is synchronous; hidden by default)
            BusyIndicator {
                anchors.centerIn: parent
                running: false
                visible: false
            }

            // Empty (no invoices at all)
            EmptyState {
                anchors.fill: parent
                visible: invoicesVm.totalCount === 0
                icon:        "🧾"
                title:       qsTr("No invoices yet")
                description: qsTr("Create your first invoice to start tracking what you're owed.")
                actionText:  qsTr("New Invoice")
                onActionClicked: root.newInvoiceRequested()
            }

            // Filtered-empty (search / status chip yields nothing)
            EmptyState {
                anchors.fill: parent
                visible: invoicesVm.totalCount > 0 && invoicesVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            // Populated list
            ListView {
                objectName: "invoiceList"
                anchors.fill: parent
                visible:  invoicesVm.totalCount > 0 && invoicesVm.filteredCount > 0
                model:    invoicesVm.listModel
                spacing:  Theme.space.sm
                clip:     true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md   // denser rows than the default lg
                    onClicked: root.rowActivated(model.invoiceId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        Avatar {
                            name: model.customer
                            diameter: 32   // slightly tighter for operational density
                        }

                        ColumnLayout {
                            spacing: Theme.space.xxs

                            Text {
                                text:  model.number
                                color: Theme.color.textPrimary
                                font.pixelSize: Theme.font.md
                                font.weight:    Theme.font.weightBold
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

                        Text {
                            text:  model.issueDate
                            color: Theme.color.textSecondary
                            font.pixelSize: Theme.font.sm
                            font.family: Theme.font.uiFamily
                        }

                        CurrencyAmount {
                            amount: model.totalText
                        }

                        StatusBadge {
                            status: model.status
                        }

                        // Trace this invoice's revenue posting into the ledger. Storage has
                        // no per-entry back-reference, so this scopes the Journal to the
                        // Revenue account (account-level, deterministic) rather than one row.
                        IconButton {
                            content:        "📒"
                            accessibleName: qsTr("View revenue in ledger")
                            onClicked:      root.viewLedgerRequested()
                        }
                    }
                }
            }
        }
    }
}
