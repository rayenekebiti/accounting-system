import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// JournalEntryInspector — read-only detail of one journal entry: its date, reversal
// lineage, balance check, and every posting (account · type · debit · credit). Bound to
// `ledgerVm.currentEntry` (set by ledgerVm.inspect). Clicking a posting's account
// re-scopes the Journal to that account. Nothing is editable.
ModalSheet {
    id: root

    objectName: "journalEntryInspectorRoot"
    title: qsTr("Journal Entry #%1").arg(entry.id !== undefined ? entry.id : 0)

    // Re-scope the Journal to a posting's account.
    signal accountActivated(int accountId)

    readonly property var entry: ledgerVm.currentEntry

    onRequestClose: root.close()

    readonly property int colType:   130
    readonly property int colAmount: 130

    // ── Meta: date + lineage + balanced ───────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xxs
            Text {
                text:  qsTr("Effective date")
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
            }
            Text {
                text:  root.entry.date !== undefined ? root.entry.date : "—"
                color: Theme.color.textPrimary
                font.pixelSize: Theme.font.md; font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily
            }
        }

        Badge {
            visible: root.entry.balanced === true
            tone: "income"
            text: qsTr("Balanced ✓")
        }
        Badge {
            visible: root.entry.isReversal === true
            tone: "pending"
            text: qsTr("Reversal of #%1").arg(root.entry.reverses !== undefined ? root.entry.reverses : 0)
        }
        Badge {
            visible: root.entry.isReversed === true
            tone: "info"
            text: qsTr("Reversed by #%1").arg(root.entry.reversedBy !== undefined ? root.entry.reversedBy : 0)
        }
    }

    Divider { Layout.fillWidth: true }

    // ── Postings header ───────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md
        Text {
            text: qsTr("Account"); Layout.fillWidth: true
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        }
        Text {
            text: qsTr("Type"); Layout.preferredWidth: root.colType
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        }
        Text {
            text: qsTr("Debit"); Layout.preferredWidth: root.colAmount
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
        }
        Text {
            text: qsTr("Credit"); Layout.preferredWidth: root.colAmount
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
        }
    }

    // ── Postings ──────────────────────────────────────────────────────────────
    Repeater {
        model: root.entry.postings !== undefined ? root.entry.postings : []
        delegate: Rectangle {
            required property var modelData
            Layout.fillWidth: true
            implicitHeight: postRow.implicitHeight + Theme.space.sm * 2
            radius: Theme.radius.md
            color:  postArea.containsMouse ? Theme.color.surfaceMuted : "transparent"

            RowLayout {
                id: postRow
                anchors.fill: parent
                anchors.leftMargin:  Theme.space.sm
                anchors.rightMargin: Theme.space.sm
                anchors.topMargin:    Theme.space.sm
                anchors.bottomMargin: Theme.space.sm
                spacing: Theme.space.md

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        text:  modelData.account
                        color: Theme.color.brand
                        font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
                        font.weight: Theme.font.weightSemibold
                        elide: Text.ElideRight; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                        Layout.fillWidth: true
                    }
                    Text {
                        text:  qsTr("view in journal →")
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                        horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                    }
                }
                Text {
                    text: modelData.typeName; Layout.preferredWidth: root.colType
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                }
                Text {
                    text: modelData.debitText; Layout.preferredWidth: root.colAmount
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.base; font.family: Theme.font.numericFamily
                    font.features: ({ "tnum": 1, "lnum": 1 }); horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
                }
                Text {
                    text: modelData.creditText; Layout.preferredWidth: root.colAmount
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.base; font.family: Theme.font.numericFamily
                    font.features: ({ "tnum": 1, "lnum": 1 }); horizontalAlignment: Text.AlignRight   // numeric column — mirrors under RTL
                }
            }

            MouseArea {
                id: postArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.accountActivated(modelData.accountId)
            }
        }
    }

    Divider { Layout.fillWidth: true }

    // ── Totals ────────────────────────────────────────────────────────────────
    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md
        Text {
            text: qsTr("Entry total"); Layout.fillWidth: true
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.base; font.weight: Theme.font.weightBold
            font.family: Theme.font.uiFamily; horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
        }
        CurrencyAmount { amount: root.entry.totalText !== undefined ? root.entry.totalText : "$0.00" }
    }

    footerData: [
        Item { Layout.fillWidth: true },
        AppButton { text: qsTr("Close"); variant: "primary"; onClicked: root.close() }
    ]
}
