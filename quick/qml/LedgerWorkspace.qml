import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// LedgerWorkspace — the read-only accounting explorer, grouped under one nav item.
// A segmented tab bar switches between the chart of Accounts, the Journal, and the
// Trial Balance (each a self-contained screen bound to its own engine-derived model).
// `activeTab` is drivable externally (nav + screenshot harness) via objectName.
Item {
    id: root
    objectName: "ledgerWorkspace"

    // "accounts" | "journal" | "trial" | "tax" | "trust" | "periods"
    property string activeTab: "accounts"

    // Forwarded to Main so the window-level inspector / tax-code editor modals can open.
    signal entryActivated(int entryId)
    signal newTaxCodeRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        // ── Segmented tab bar ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Chip {
                text: qsTr("Accounts");      selected: root.activeTab === "accounts"
                onClicked: root.activeTab = "accounts"
            }
            Chip {
                text: qsTr("Journal");       selected: root.activeTab === "journal"
                onClicked: root.activeTab = "journal"
            }
            Chip {
                text: qsTr("Trial Balance"); selected: root.activeTab === "trial"
                onClicked: root.activeTab = "trial"
            }
            Chip {
                text: qsTr("Tax"); selected: root.activeTab === "tax"
                onClicked: root.activeTab = "tax"
            }
            Chip {
                text: qsTr("Trust"); selected: root.activeTab === "trust"
                onClicked: root.activeTab = "trust"
            }
            Chip {
                text: qsTr("Periods"); selected: root.activeTab === "periods"
                onClicked: root.activeTab = "periods"
            }

            Item { Layout.fillWidth: true }
        }

        Divider { Layout.fillWidth: true }

        // ── Active screen ─────────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            currentIndex: root.activeTab === "journal" ? 1
                        : root.activeTab === "trial"   ? 2
                        : root.activeTab === "tax"     ? 3
                        : root.activeTab === "trust"   ? 4
                        : root.activeTab === "periods" ? 5 : 0

            // index 0 — Accounts (row → scope the Journal to that account)
            AccountsScreen {
                onRowActivated: (accountId) => {
                    ledgerVm.setAccountScope(accountId)
                    root.activeTab = "journal"
                }
            }

            // index 1 — Journal (row → inspect entry, handled at window level)
            LedgerExplorer {
                onEntryActivated: (entryId) => root.entryActivated(entryId)
            }

            // index 2 — Trial Balance
            TrialBalanceScreen {}

            // index 3 — Tax (VAT/GST summary + tax-code registry)
            TaxSummaryScreen {
                onNewTaxCodeRequested: root.newTaxCodeRequested()
            }

            // index 4 — Trust & Status (read-only confidence projections)
            TrustDashboard {}

            // index 5 — Accounting Periods (close/reopen a filed month/quarter)
            PeriodCloseScreen {}
        }
    }
}
