import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// ExpensesScreen — lists expenses from the authoritative projection. Every value is
// engine-derived; recording/voiding go through `expenseEditor` (event-authored). A row edits;
// a Void action (open period) marks it void + posts a compensating ledger entry.
Item {
    id: root

    signal newExpenseRequested()
    signal rowActivated(int expenseId)
    signal voidRequested(int expenseId)

    property string activeCategory: "All"

    // Map the engine's category KEY (kept English for filtering) to a translated label.
    // Referencing i18n.language makes the binding re-run on a live language switch.
    function catLabel(c) {
        i18n.language
        return c === "Office"    ? qsTr("Office")
             : c === "Rent"      ? qsTr("Rent")
             : c === "Utilities" ? qsTr("Utilities")
             : c === "Travel"    ? qsTr("Travel")
             : qsTr("Other")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Expenses")
            subtitle: qsTr("%n expense(s)", "", expensesVm.totalCount)

            AppButton {
                text:    qsTr("New Expense")
                variant: "primary"
                onClicked: root.newExpenseRequested()
            }
        }

        SummaryBar {
            objectName: "expenseSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "expenseHeroCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: qsTr("Spent")
                value: expensesVm.totalSpentText
                tone:  "expense"
                sub:   qsTr("%n expense(s)", "", expensesVm.expenseCount)
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Cash")
                value: expensesVm.cashText
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("On credit")
                value: expensesVm.creditText
                sub:   qsTr("payable")
            }
        }

        FilterBar {
            Layout.fillWidth: true
            searchPlaceholder: qsTr("Search expenses…")
            onSearchTextChanged: expensesVm.setSearchText(searchText)

            Chip { text: qsTr("All");       selected: root.activeCategory === "All";       onClicked: { root.activeCategory = "All";       expensesVm.setCategoryFilter("All") } }
            Chip { text: qsTr("Office");    selected: root.activeCategory === "Office";    onClicked: { root.activeCategory = "Office";    expensesVm.setCategoryFilter("Office") } }
            Chip { text: qsTr("Rent");      selected: root.activeCategory === "Rent";      onClicked: { root.activeCategory = "Rent";      expensesVm.setCategoryFilter("Rent") } }
            Chip { text: qsTr("Utilities"); selected: root.activeCategory === "Utilities"; onClicked: { root.activeCategory = "Utilities"; expensesVm.setCategoryFilter("Utilities") } }
            Chip { text: qsTr("Travel");    selected: root.activeCategory === "Travel";    onClicked: { root.activeCategory = "Travel";    expensesVm.setCategoryFilter("Travel") } }
            Chip { text: qsTr("Credit");    selected: root.activeCategory === "Credit";    onClicked: { root.activeCategory = "Credit";    expensesVm.setCategoryFilter("Credit") } }
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: expensesVm.totalCount === 0
                icon:        "💸"
                title:       qsTr("No expenses yet")
                description: qsTr("Record an expense — it posts to the ledger and your income statement.")
                actionText:  qsTr("New Expense")
                onActionClicked: root.newExpenseRequested()
            }

            EmptyState {
                anchors.fill: parent
                visible: expensesVm.totalCount > 0 && expensesVm.filteredCount === 0
                icon:        "🔍"
                title:       qsTr("No matches")
                description: qsTr("Try a different search or filter.")
            }

            ListView {
                objectName: "expenseList"
                anchors.fill: parent
                visible: expensesVm.totalCount > 0 && expensesVm.filteredCount > 0
                model:   expensesVm.listModel
                spacing: Theme.space.sm
                clip:    true

                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md
                    onClicked: if (!model.isVoid && !model.isReversal) root.rowActivated(model.expenseId)

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.xxs

                            RowLayout {
                                spacing: Theme.space.sm
                                Text {
                                    text:            model.supplier
                                    color:           Theme.color.textPrimary
                                    font.pixelSize:  Theme.font.md
                                    font.weight:     Theme.font.weightBold
                                    font.family:     Theme.font.uiFamily
                                    font.strikeout:  model.isVoid
                                    elide:           Text.ElideRight
                                    horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                }
                                Badge { tone: "neutral"; text: root.catLabel(model.category) }
                            }
                            Text {
                                text:            model.date + " · " + model.paymentMethod
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                        }

                        Badge { visible: model.isVoid;     tone: "pending"; text: qsTr("Void") }
                        Badge { visible: model.isReversal; tone: "info";    text: qsTr("Reversal") }
                        Badge { visible: model.isReversed; tone: "info";    text: qsTr("Reversed") }

                        CurrencyAmount { amount: model.amountText }

                        AppButton {
                            visible: !model.isVoid && !model.isReversal
                            text:    qsTr("Void")
                            variant: "ghost"
                            onClicked: root.voidRequested(model.expenseId)
                        }
                    }
                }
            }
        }
    }
}
