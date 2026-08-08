import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// AccountsScreen — the chart of accounts, read-only. Every value (balance, per-class
// rollups, trial-balance status) is derived by the ledger engine and surfaced via
// `accountsVm` (AccountsViewModel). A row opens that account in the Journal tab.
Item {
    id: root

    signal rowActivated(int accountId)

    property string activeCategory: "All"

    function typeTone(t) {
        return t === "Asset"     ? "info"
             : t === "Liability" ? "pending"
             : t === "Equity"    ? "brand"
             : t === "Income"    ? "income"
             : "expense"
    }

    // Map the engine's account-type / normal-side KEYS (kept English for tone + logic) to
    // translated labels. Referencing i18n.language re-runs the binding on a live switch.
    function typeLabel(t) {
        i18n.language
        return t === "Asset"     ? qsTr("Asset")
             : t === "Liability" ? qsTr("Liability")
             : t === "Equity"    ? qsTr("Equity")
             : t === "Income"    ? qsTr("Income")
             : qsTr("Expense")
    }
    function sideLabel(s) {
        i18n.language
        return s === "Debit" ? qsTr("Debit") : qsTr("Credit")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Chart of Accounts")
            subtitle: qsTr("%n account(s)", "", accountsVm.totalCount)

            Badge {
                tone: accountsVm.isBalanced ? "income" : "expense"
                text: accountsVm.isBalanced
                      ? qsTr("Balanced ✓  %1").arg(accountsVm.trialBalanceText)
                      : qsTr("Unbalanced  %1").arg(accountsVm.trialBalanceText)
            }
        }

        SummaryBar {
            objectName: "accountsSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "accountsHeroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Accounts")
                value: String(accountsVm.totalCount)
                sub:   qsTr("trial balance %1").arg(accountsVm.trialBalanceText)
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Assets")
                value: accountsVm.assetsText
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Liabilities + Equity")
                value: accountsVm.liabilitiesText
                sub:   qsTr("equity %1").arg(accountsVm.equityText)
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Income")
                value: accountsVm.incomeText
                tone:  "income"
                sub:   qsTr("expense %1").arg(accountsVm.expenseText)
            }
        }

        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search accounts…")
            onSearchTextChanged: accountsVm.setSearchText(searchText)

            Chip { text: qsTr("All");       selected: root.activeCategory === "All";       onClicked: { root.activeCategory = "All";       accountsVm.setCategoryFilter("All") } }
            Chip { text: qsTr("Asset");     selected: root.activeCategory === "Asset";     onClicked: { root.activeCategory = "Asset";     accountsVm.setCategoryFilter("Asset") } }
            Chip { text: qsTr("Liability"); selected: root.activeCategory === "Liability"; onClicked: { root.activeCategory = "Liability"; accountsVm.setCategoryFilter("Liability") } }
            Chip { text: qsTr("Equity");    selected: root.activeCategory === "Equity";    onClicked: { root.activeCategory = "Equity";    accountsVm.setCategoryFilter("Equity") } }
            Chip { text: qsTr("Income");    selected: root.activeCategory === "Income";    onClicked: { root.activeCategory = "Income";    accountsVm.setCategoryFilter("Income") } }
            Chip { text: qsTr("Expense");   selected: root.activeCategory === "Expense";   onClicked: { root.activeCategory = "Expense";   accountsVm.setCategoryFilter("Expense") } }
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: accountsVm.totalCount === 0
                icon:        "📒"
                title:       qsTr("No accounts yet")
                description: qsTr("The chart of accounts is created automatically when the books open.")
            }

            EmptyState {
                anchors.fill: parent
                visible: accountsVm.totalCount > 0 && accountsVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            ListView {
                objectName: "accountList"
                anchors.fill: parent
                visible: accountsVm.totalCount > 0 && accountsVm.filteredCount > 0
                model:   accountsVm.listModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: root.rowActivated(model.accountId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

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
                                text:            qsTr("%1 · normal side %2").arg(root.typeLabel(model.typeName)).arg(root.sideLabel(model.normalSide))
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                        }

                        Badge {
                            tone: root.typeTone(model.typeName)
                            text: root.typeLabel(model.typeName)
                        }

                        ColumnLayout {
                            spacing: Theme.space.xxs
                            CurrencyAmount { amount: model.balanceText; Layout.alignment: Qt.AlignRight }
                            Text {
                                // The side the balance is CURRENTLY on (signed: Dr+ / Cr−), so an
                                // account sitting opposite its normal side (e.g. overdrawn Cash) reads
                                // correctly; falls back to the account's normal side at zero.
                                text:  model.balanceCents > 0 ? qsTr("Dr")
                                     : model.balanceCents < 0 ? qsTr("Cr")
                                     : (model.normalSide === "Debit" ? qsTr("Dr") : qsTr("Cr"))
                                color: Theme.color.textSecondary
                                font.pixelSize: Theme.font.xs
                                font.family:    Theme.font.uiFamily
                                Layout.alignment: Qt.AlignRight
                            }
                        }
                    }
                }
            }
        }
    }
}
