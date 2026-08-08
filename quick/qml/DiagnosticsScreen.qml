import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// DiagnosticsScreen — a READ-ONLY view of the engine's health: version contract, storage
// footprint, history size, and on-demand integrity verification. Nothing here mutates the
// store; "Run verification" re-derives the projection from authoritative history and compares
// (non-destructive). Every value comes from diagnosticsVm.
ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.space.lg

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Diagnostics")
            subtitle: qsTr("Read-only engine and data health.")

            AppButton {
                text:    qsTr("Run verification")
                variant: "secondary"
                loading: diagnosticsVm.verifying
                accessibleName: qsTr("Re-verify data against its authoritative history")
                onClicked: diagnosticsVm.runVerification()
            }
        }

        // ── Metrics grid ──────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            GridLayout {
                width: parent.width
                columns: 2
                columnSpacing: Theme.space.xxl
                rowSpacing: Theme.space.lg

                MetricCell { Layout.fillWidth: true; label: qsTr("Engine");             value: diagnosticsVm.engineVersion }
                MetricCell { Layout.fillWidth: true; label: qsTr("Posting policy");      value: diagnosticsVm.postingPolicy }
                MetricCell { Layout.fillWidth: true; label: qsTr("Version contract");    value: diagnosticsVm.compatVersion }
                MetricCell { Layout.fillWidth: true; label: qsTr("Compatibility");       value: diagnosticsVm.compatStatus }
                MetricCell { Layout.fillWidth: true; label: qsTr("Snapshot");            value: diagnosticsVm.snapshotStatus }
                MetricCell { Layout.fillWidth: true; label: qsTr("Database size");       value: diagnosticsVm.databaseSize }
                MetricCell { Layout.fillWidth: true; label: qsTr("Events recorded");     value: diagnosticsVm.eventCount }
                MetricCell { Layout.fillWidth: true; label: qsTr("Current sequence");    value: diagnosticsVm.currentSeq }
                MetricCell { Layout.fillWidth: true; label: qsTr("Ledger");              value: diagnosticsVm.accountCount }
                MetricCell {
                    Layout.fillWidth: true
                    label: qsTr("Trial balance"); value: diagnosticsVm.trialBalance
                    tone:  diagnosticsVm.trialBalanceOk ? "income" : "expense"
                }
                MetricCell { Layout.fillWidth: true; label: qsTr("Last backup");         value: diagnosticsVm.lastBackup }
            }
        }

        // ── Verification results ──────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Integrity verification")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("Rebuilds your data from its authoritative history and confirms the live "
                             + "figures match exactly. This is read-only and safe to run at any time.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    Text {
                        text: qsTr("Projection")
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        Layout.preferredWidth: 110
                    }
                    Badge {
                        tone: diagnosticsVm.projectionOk ? "income" : "expense"
                        text: diagnosticsVm.projectionResult
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    Text {
                        text: qsTr("Replay")
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        Layout.preferredWidth: 110
                    }
                    Badge {
                        tone: diagnosticsVm.replayOk ? "income" : "expense"
                        text: diagnosticsVm.replayResult
                    }
                }
            }
        }

        // ── Support ───────────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Support")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("If you contact support, share this ID and, if asked, a diagnostics bundle. "
                             + "The bundle contains app health only — never your accounting data, and "
                             + "nothing is sent automatically.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    Text {
                        text: qsTr("Support ID")
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        Layout.preferredWidth: 110
                    }
                    TextEdit {
                        id: supportIdField
                        text: diagnosticsVm.supportId
                        readOnly: true
                        selectByMouse: true
                        Accessible.name: qsTr("Support ID")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.base; font.weight: Theme.font.weightSemibold
                        font.family: Theme.font.uiFamily
                    }
                    AppButton {
                        text: qsTr("Copy"); variant: "ghost"; size: "sm"
                        accessibleName: qsTr("Copy the support ID")
                        onClicked: { supportIdField.selectAll(); supportIdField.copy(); supportIdField.deselect() }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    AppButton {
                        text:    qsTr("Create support bundle")
                        variant: "secondary"
                        loading: diagnosticsVm.bundleBusy
                        accessibleName: qsTr("Create a diagnostics bundle to send to support")
                        onClicked: diagnosticsVm.exportSupportBundle()
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: diagnosticsVm.lastBundlePath.length > 0
                        text: qsTr("Saved to: %1").arg(diagnosticsVm.lastBundlePath)
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
