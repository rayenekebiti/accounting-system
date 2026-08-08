import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// TrialBalanceScreen — one row per account with its debit/credit column, straight from
// the ledger (`trialBalanceModel`, TrialBalanceModel). Total debit == total credit always,
// because the trial balance is 0 by construction. Read-only.
Item {
    id: root

    readonly property int colType:   140
    readonly property int colAmount:  150

    // Map the engine's account-type KEY (English) to a translated label; i18n.language
    // dependency re-runs the binding on a live language switch.
    function typeLabel(t) {
        i18n.language
        return t === "Asset"     ? qsTr("Asset")
             : t === "Liability" ? qsTr("Liability")
             : t === "Equity"    ? qsTr("Equity")
             : t === "Income"    ? qsTr("Income")
             : qsTr("Expense")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Trial Balance")
            subtitle: qsTr("%n account(s)", "", trialBalanceModel.accountCount)

            Badge {
                tone: trialBalanceModel.balanced ? "income" : "expense"
                text: trialBalanceModel.balanced
                      ? qsTr("Balanced ✓  (difference %1)").arg(trialBalanceModel.differenceText)
                      : qsTr("Out of balance  (difference %1)").arg(trialBalanceModel.differenceText)
            }

            AppButton {
                text:    qsTr("Export CSV")
                variant: "secondary"
                onClicked: { exportVm.exportTrialBalance(); exportVm.openExportsFolder() }
            }
        }

        // Column header
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin:  Theme.space.md
            Layout.rightMargin: Theme.space.md
            spacing: Theme.space.md

            Text {
                text: qsTr("Account"); Layout.fillWidth: true
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
                font.family: Theme.font.uiFamily; font.letterSpacing: 0.5
                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            }
            Text {
                text: qsTr("Type"); Layout.preferredWidth: root.colType
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
                font.family: Theme.font.uiFamily; font.letterSpacing: 0.5
                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            }
            Text {
                text: qsTr("Debit"); Layout.preferredWidth: root.colAmount
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
                font.family: Theme.font.uiFamily; font.letterSpacing: 0.5
                horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
            }
            Text {
                text: qsTr("Credit"); Layout.preferredWidth: root.colAmount
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
                font.family: Theme.font.uiFamily; font.letterSpacing: 0.5
                horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
            }
        }

        Divider { Layout.fillWidth: true }

        ListView {
            objectName: "trialBalanceList"
            Layout.fillWidth:  true
            Layout.fillHeight: true
            model:   trialBalanceModel
            clip:    true
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                width:  ListView.view ? ListView.view.width : 0
                height: rowLayout.implicitHeight + Theme.space.sm * 2
                color:  (index % 2 === 0) ? "transparent" : Theme.color.surfaceMuted

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin:  Theme.space.md
                    anchors.rightMargin: Theme.space.md
                    anchors.topMargin:    Theme.space.sm
                    anchors.bottomMargin: Theme.space.sm
                    spacing: Theme.space.md

                    Text {
                        text: model.name; Layout.fillWidth: true
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
                        elide: Text.ElideRight; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                    }
                    Text {
                        text: root.typeLabel(model.typeName); Layout.preferredWidth: root.colType
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                    }
                    Text {
                        text: model.debitText; Layout.preferredWidth: root.colAmount
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.base; font.family: Theme.font.numericFamily
                        font.features: ({ "tnum": 1, "lnum": 1 })
                        horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL   // numeric column — mirrors under RTL
                    }
                    Text {
                        text: model.creditText; Layout.preferredWidth: root.colAmount
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.base; font.family: Theme.font.numericFamily
                        font.features: ({ "tnum": 1, "lnum": 1 })
                        horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL   // numeric column — mirrors under RTL
                    }
                }
            }
        }

        Divider { Layout.fillWidth: true }

        // Totals row
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin:  Theme.space.md
            Layout.rightMargin: Theme.space.md
            spacing: Theme.space.md

            Text {
                text: qsTr("Total"); Layout.fillWidth: true
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.base; font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
            }
            Item { Layout.preferredWidth: root.colType }
            Text {
                text: trialBalanceModel.totalDebitText; Layout.preferredWidth: root.colAmount
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.base; font.weight: Theme.font.weightBold
                font.family: Theme.font.numericFamily; font.features: ({ "tnum": 1, "lnum": 1 })
                horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
            }
            Text {
                text: trialBalanceModel.totalCreditText; Layout.preferredWidth: root.colAmount
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.base; font.weight: Theme.font.weightBold
                font.family: Theme.font.numericFamily; font.features: ({ "tnum": 1, "lnum": 1 })
                horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
            }
        }
    }
}
