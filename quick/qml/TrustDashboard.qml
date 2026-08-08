import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// TrustDashboard — a read-only confidence panel. Every value is a projection already exposed by
// diagnosticsVm (books/verification/integrity/backup/trial balance) and platform (license/update).
// It authors nothing and mutates nothing; it only reads. Shown as a tab in the Ledger/Reports area.
Item {
    id: root

    function refreshAll() { diagnosticsVm.refresh() }
    Component.onCompleted: refreshAll()

    // One trust row: label + value + a status dot (ok/warn).
    component TrustRow: RowLayout {
        property string label: ""
        property string value: ""
        property bool   ok: true
        property bool   neutral: false
        Layout.fillWidth: true
        spacing: Theme.space.md

        Rectangle {
            width: 10; height: 10; radius: 5
            color: parent.neutral ? Theme.color.textSecondary
                                   : (parent.ok ? Theme.color.income : Theme.color.expense)
        }
        Text {
            text: parent.label
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm
            font.family: Theme.font.uiFamily
            Layout.preferredWidth: 220
        }
        Text {
            text: parent.value
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.sm
            font.family: Theme.font.uiFamily
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.md
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Trust & Status")
            subtitle: qsTr("A read-only view of your data's health")

            AppButton {
                text: qsTr("Refresh")
                variant: "secondary"
                onClicked: root.refreshAll()
            }
        }

        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm

                TrustRow {
                    objectName: "trustTrialBalance"
                    label: qsTr("Trial balance")
                    value: diagnosticsVm.trialBalanceOk ? qsTr("Balanced ✓") : qsTr("Out of balance")
                    ok: diagnosticsVm.trialBalanceOk
                }
                Divider { Layout.fillWidth: true }
                TrustRow {
                    label: qsTr("Book verification")
                    value: diagnosticsVm.compatStatus
                    ok: true
                }
                Divider { Layout.fillWidth: true }
                TrustRow {
                    objectName: "trustIntegrity"
                    label: qsTr("Data integrity (history)")
                    value: qsTr("%1 events · seq %2").arg(diagnosticsVm.eventCount).arg(diagnosticsVm.currentSeq)
                    ok: true
                }
                Divider { Layout.fillWidth: true }
                TrustRow {
                    objectName: "trustBackup"
                    label: qsTr("Last successful backup")
                    value: diagnosticsVm.lastBackup
                    neutral: diagnosticsVm.lastBackup.length === 0
                    ok: diagnosticsVm.lastBackup.length > 0
                }
                Divider { Layout.fillWidth: true }
                TrustRow {
                    objectName: "trustLicense"
                    label: qsTr("License")
                    value: platform.licenseDetail
                    ok: platform.licenseValid
                }
                Divider { Layout.fillWidth: true }
                TrustRow {
                    label: qsTr("Updates")
                    // Localized in QML (direct qsTr retranslates live) — never the raw English
                    // platform.updateStatusText, and honest about how v1 installs (via the installer).
                    value: platform.updateState === "Error"
                           ? qsTr("Update check failed — your current version is unaffected.")
                           : platform.updateStaged
                             ? qsTr("Downloaded — run the installer to finish updating.")
                             : platform.updateAvailable
                               ? qsTr("Version %1 available.").arg(platform.availableVersion)
                               : platform.updateState === "UpToDate"
                                 ? qsTr("Up to date.")
                                 : qsTr("Checked locally.")
                    neutral: !platform.updateAvailable
                    ok: platform.updateState !== "Error"
                }
            }
        }

        // Deep verification (non-destructive) — reuses the existing engine gate.
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm
                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: qsTr("Deep verification")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.md
                        font.bold: true
                        font.family: Theme.font.uiFamily
                        Layout.fillWidth: true
                    }
                    AppButton {
                        text: diagnosticsVm.verifying ? qsTr("Verifying…") : qsTr("Verify now")
                        variant: "primary"
                        enabled: !diagnosticsVm.verifying
                        onClicked: diagnosticsVm.runVerification()
                    }
                }
                TrustRow {
                    label: qsTr("Live projection == history")
                    value: diagnosticsVm.projectionResult
                    ok: diagnosticsVm.projectionOk
                }
                TrustRow {
                    label: qsTr("Deterministic replay")
                    value: diagnosticsVm.replayResult
                    ok: diagnosticsVm.replayOk
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
