import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// RecoveryBlocker — a BLOCKING startup screen shown when crash recovery ran but re-verification
// found the live data does NOT match its authoritative history. We never continue silently on
// suspect books: the only actions are to quit (and restore a backup) — there is no "continue".
// Cannot be dismissed (no Esc / click-away). Detail text comes from settingsVm.
Popup {
    id: root

    modal:       true
    dim:         true
    focus:       true
    closePolicy: Popup.NoAutoClose

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min((Overlay.overlay ? Overlay.overlay.width : 520) * 0.92, 520)

    onOpened: quitBtn.forceActiveFocus()

    Overlay.modal: Rectangle {
        color: Qt.rgba(Theme.color.canvas.r, Theme.color.canvas.g, Theme.color.canvas.b, 0.75)
    }

    background: Rectangle {
        radius:       Theme.radius.card
        color:        Theme.color.surface
        border.color: Theme.color.expense
        border.width: 2
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        RowLayout {
            Layout.topMargin:   Theme.space.lg
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            Layout.fillWidth:   true
            spacing: Theme.space.sm

            Text {
                text: "⚠"
                color: Theme.color.expense
                font.pixelSize: Theme.font.xxl; font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Data verification failed")
                color: Theme.color.expense
                font.pixelSize: Theme.font.lg; font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily
                wrapMode: Text.WordWrap
            }
        }

        Text {
            Layout.fillWidth:   true
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            text: settingsVm.recoveryDetail.length > 0
                  ? settingsVm.recoveryDetail
                  : qsTr("Automatic verification found an inconsistency between your live data and its "
                       + "authoritative history. To protect your books, the application will not continue.")
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth:   true
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            text: qsTr("Recommended: quit now and restore your most recent backup, then reopen the "
                     + "application. Contact support if the problem persists.")
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
            wrapMode: Text.WordWrap
        }

        Divider {}

        RowLayout {
            Layout.fillWidth:    true
            Layout.bottomMargin: Theme.space.lg
            Layout.leftMargin:   Theme.space.xl
            Layout.rightMargin:  Theme.space.xl
            spacing: Theme.space.sm

            Item { Layout.fillWidth: true }

            AppButton {
                id: quitBtn
                text: qsTr("Quit")
                variant: "danger"
                accessibleName: qsTr("Quit the application")
                onClicked: Qt.quit()
            }
        }
    }
}
