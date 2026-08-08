import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// PeriodCloseScreen — the on-screen form onto the EXISTING engine capability
// (periodVm → AuditJournal::closePeriod / reopenPeriod). It adds no accounting behaviour:
// closing/reopening a period is already an authoritative, append-only event, and the engine
// already refuses edits/voids whose effective date falls in a closed period. This screen only
// lets a user actually freeze a filed month/quarter and reopen it by label. Nothing is cached —
// closedCount is read live from the authoritative history.
Item {
    id: root
    objectName: "periodCloseScreen"

    // Transient status line (success or friendly error), cleared on a new attempt.
    property string statusText: ""
    property bool   statusIsError: false

    // Map the VM's terse reason codes to friendly, translatable copy; fall through to the raw
    // engine message for anything unmapped (still readable, never a bare code with no text).
    function friendly(reason) {
        i18n.language   // dependency: retranslate on a live language switch
        if (reason === "labelRequired") return qsTr("Enter a label for the period.")
        if (reason === "invalidDate")   return qsTr("Enter valid dates as YYYY-MM-DD.")
        if (reason === "notReady")      return qsTr("The books aren't ready yet. Try again in a moment.")
        return reason
    }

    Connections {
        target: periodVm
        function onActionFailed(reason) {
            root.statusIsError = true
            root.statusText = root.friendly(reason)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Accounting Periods")
            subtitle: qsTr("Freeze a filed month or quarter so its entries can't be changed by accident.")

            Badge {
                tone: "info"
                text: qsTr("Currently closed periods: %1").arg(periodVm.closedCount)
            }
        }

        // Transient status line (success / error).
        Text {
            Layout.fillWidth: true
            visible: root.statusText.length > 0
            text:    root.statusText
            color:   root.statusIsError ? Theme.color.expense : Theme.color.income
            font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
            font.weight: Theme.font.weightSemibold
            wrapMode: Text.WordWrap
        }

        // ── Close a period ──────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Close a period")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.base; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Closing is append-only and reversible — you can reopen it below. After a "
                               + "period is closed, corrections to it must be posted as reversals, never edits.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap
                }

                AppTextField {
                    id: labelField
                    Layout.fillWidth: true
                    label:       qsTr("Period label")
                    placeholder: qsTr("e.g. 2026-Q2 or July 2026")
                    required:    true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    AppTextField {
                        id: startField
                        Layout.fillWidth: true
                        label:       qsTr("Start date")
                        placeholder: "YYYY-MM-DD"
                        required:    true
                    }
                    AppTextField {
                        id: endField
                        Layout.fillWidth: true
                        label:       qsTr("End date")
                        placeholder: "YYYY-MM-DD"
                        required:    true
                    }
                }

                AppButton {
                    text:    qsTr("Close Period")
                    variant: "primary"
                    accessibleName: qsTr("Close the period with the entered label and dates")
                    onClicked: {
                        root.statusText = ""
                        if (periodVm.closePeriod(labelField.text, startField.text, endField.text)) {
                            root.statusIsError = false
                            root.statusText = qsTr("Period frozen: %1").arg(labelField.text.trim())
                            labelField.text = ""; startField.text = ""; endField.text = ""
                        }
                    }
                }
            }
        }

        // ── Reopen a period ─────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Reopen a period")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.base; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Enter the exact label you used when closing to unfreeze it.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    AppTextField {
                        id: reopenField
                        Layout.fillWidth: true
                        label:       qsTr("Period label")
                        placeholder: qsTr("e.g. 2026-Q2 or July 2026")
                    }
                    AppButton {
                        Layout.alignment: Qt.AlignBottom
                        text:    qsTr("Reopen Period")
                        variant: "secondary"
                        accessibleName: qsTr("Reopen the period with the entered label")
                        onClicked: {
                            root.statusText = ""
                            if (periodVm.reopenPeriod(reopenField.text)) {
                                root.statusIsError = false
                                root.statusText = qsTr("Period reopened: %1").arg(reopenField.text.trim())
                                reopenField.text = ""
                            }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
