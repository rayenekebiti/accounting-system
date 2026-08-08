import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// LedgerExplorer — the Journal: every balanced journal entry from the ledger engine,
// read-only. Optionally scoped to one account (via `ledgerVm.setAccountScope`). A row
// opens the JournalEntryInspector. Bound to `ledgerVm` (LedgerExplorerViewModel).
Item {
    id: root

    signal entryActivated(int entryId)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Journal")
            subtitle: ledgerVm.hasScope
                      ? qsTr("%n entry(s) touching this account", "", ledgerVm.filteredCount)
                      : qsTr("%n journal entry(s)", "", ledgerVm.entryCount)
        }

        // Account scope banner (shown only when scoped from Accounts / a posting).
        Card {
            objectName: "ledgerScopeBanner"
            Layout.fillWidth: true
            visible: ledgerVm.hasScope

            RowLayout {
                width: parent.width
                spacing: Theme.space.md

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.xxs
                    Text {
                        text:  qsTr("Account: %1").arg(ledgerVm.scopeAccountName)
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.md
                        font.weight:    Theme.font.weightBold
                        font.family:    Theme.font.uiFamily
                        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                    }
                    Text {
                        text:  qsTr("Balance %1 %2").arg(ledgerVm.scopeBalanceText).arg(ledgerVm.scopeSideText)
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm
                        font.family:    Theme.font.uiFamily
                        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                    }
                }

                AppButton {
                    text:    qsTr("Clear scope")
                    variant: "ghost"
                    onClicked: ledgerVm.clearScope()
                }
            }
        }

        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search entries…")
            onSearchTextChanged: ledgerVm.setSearchText(searchText)
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: ledgerVm.entryCount === 0
                icon:        "📖"
                title:       qsTr("No journal entries yet")
                description: qsTr("Posting an invoice records a balanced revenue entry here.")
            }

            EmptyState {
                anchors.fill: parent
                visible: ledgerVm.entryCount > 0 && ledgerVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search, or clear the account scope.")
            }

            ListView {
                objectName: "ledgerList"
                anchors.fill: parent
                visible: ledgerVm.entryCount > 0 && ledgerVm.filteredCount > 0
                model:   ledgerVm.entriesModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: root.entryActivated(model.entryId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.xxs

                            RowLayout {
                                spacing: Theme.space.sm
                                Text {
                                    text:            model.description
                                    color:           Theme.color.textPrimary
                                    font.pixelSize:  Theme.font.md
                                    font.weight:     Theme.font.weightBold
                                    font.family:     Theme.font.uiFamily
                                    horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                }
                                Badge { visible: model.isReversal; tone: "pending"; text: qsTr("Reversal") }
                                Badge { visible: model.isReversed; tone: "info";    text: qsTr("Reversed") }
                            }
                            Text {
                                text:            qsTr("%1 · %n posting(s)", "", model.lineCount).arg(model.date)
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                        }

                        // When scoped: this account's signed movement; else the entry total.
                        ColumnLayout {
                            spacing: Theme.space.xxs
                            Text {
                                visible: ledgerVm.hasScope
                                text:    model.scopedAmountText
                                color:   Theme.color.textPrimary
                                font.pixelSize: Theme.font.md
                                font.weight:    Theme.font.weightBold
                                font.family:    Theme.font.numericFamily
                                Layout.alignment: Qt.AlignRight
                            }
                            CurrencyAmount { visible: !ledgerVm.hasScope; amount: model.debitTotalText; Layout.alignment: Qt.AlignRight }
                            Text {
                                text:  qsTr("entry #%1").arg(model.entryId)
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
