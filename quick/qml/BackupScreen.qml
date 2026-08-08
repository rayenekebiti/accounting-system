import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// BackupScreen — manual backup, restore, and verification over the on-disk data set.
// A backup is a full, self-contained copy of your books; a restore replaces the current
// data with a chosen backup and takes effect after a restart. Internal file names are never
// shown — a backup is presented only as "date (size)". All work goes through backupVm.
Item {
    id: root

    // Name (opaque timestamp folder) of the backup a restore was requested for.
    property string pendingRestore: ""

    ConfirmDialog {
        id: confirmRestore
        title:          qsTr("Restore this backup?")
        message:        qsTr("This replaces ALL current data with the selected backup. Any changes made "
                             + "since that backup will be lost. The restore is applied when you reopen the "
                             + "application. This cannot be undone.")
        confirmText:    qsTr("Restore")
        cancelText:     qsTr("Cancel")
        confirmVariant: "danger"
        onConfirmed: backupVm.restore(root.pendingRestore)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Backup & Restore")
            subtitle: qsTr("Keep safe copies of your books. Last backup: %1.").arg(backupVm.lastBackupText)

            AppButton {
                text:    qsTr("Back Up Now")
                variant: "primary"
                loading: backupVm.busy
                accessibleName: qsTr("Create a new backup now")
                onClicked: backupVm.backupNow()
            }
        }

        // Estimated size + status line
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Badge {
                tone: "info"
                text: qsTr("Estimated backup size: %1").arg(backupVm.estimatedSize)
            }
            Text {
                Layout.fillWidth: true
                visible: backupVm.statusText.length > 0
                text: backupVm.statusText
                color: Theme.color.textSecondary
                font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                wrapMode: Text.WordWrap
            }
        }

        // Restart-required banner (after a restore is staged)
        Rectangle {
            Layout.fillWidth: true
            visible: backupVm.restartRequired
            radius: Theme.radius.md
            color:  Theme.color.pendingSubtle
            border.color: Theme.color.pending; border.width: 1
            implicitHeight: bannerText.implicitHeight + Theme.space.md * 2
            Text {
                id: bannerText
                anchors.fill: parent
                anchors.margins: Theme.space.md
                text: qsTr("A restore is ready. Close and reopen Occountant to complete it.")
                color: Theme.color.pending
                font.pixelSize: Theme.font.sm; font.weight: Theme.font.weightSemibold
                font.family: Theme.font.uiFamily
                wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter
            }
        }

        Divider { Layout.fillWidth: true }

        Text {
            text: qsTr("Backup history")
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily; font.letterSpacing: 0.5
        }

        // ── History list ──────────────────────────────────────────────────────
        ListView {
            objectName: "backupList"
            Layout.fillWidth:  true
            Layout.fillHeight: true
            model: backupVm.backups
            clip:  true
            spacing: Theme.space.sm
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                required property var modelData
                width:  ListView.view ? ListView.view.width : 0
                height: rowLayout.implicitHeight + Theme.space.md * 2
                radius: Theme.radius.md
                color:  Theme.color.surface
                border.color: Theme.color.border; border.width: 1

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.leftMargin:  Theme.space.lg
                    anchors.rightMargin: Theme.space.lg
                    anchors.topMargin:    Theme.space.md
                    anchors.bottomMargin: Theme.space.md
                    spacing: Theme.space.md

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space.xxs
                        Text {
                            text: qsTr("Backup from %1").arg(modelData.dateText)
                            color: Theme.color.textPrimary
                            font.pixelSize: Theme.font.base; font.weight: Theme.font.weightSemibold
                            font.family: Theme.font.uiFamily
                        }
                        Text {
                            text: qsTr("Size: %1").arg(modelData.sizeText)
                            color: Theme.color.textSecondary
                            font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                        }
                    }

                    AppButton {
                        text: qsTr("Verify"); variant: "ghost"; size: "sm"
                        accessibleName: qsTr("Verify this backup's integrity")
                        onClicked: backupVm.verify(modelData.name)
                    }
                    AppButton {
                        text: qsTr("Restore"); variant: "secondary"; size: "sm"
                        accessibleName: qsTr("Restore this backup")
                        onClicked: { root.pendingRestore = modelData.name; confirmRestore.open() }
                    }
                }
            }

            // Empty state
            EmptyState {
                anchors.centerIn: parent
                width: parent.width * 0.8
                visible: backupVm.backups.length === 0
                icon:  "🗄"
                title: qsTr("No backups yet")
                description: qsTr("Create your first backup so you always have a safe copy of your books.")
                actionText: qsTr("Back Up Now")
                onActionClicked: backupVm.backupNow()
            }
        }
    }
}
