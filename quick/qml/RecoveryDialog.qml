import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// RecoveryDialog — a brief, reassuring notice shown once at startup when the engine recovered
// from a prior crash (replayed a journal / discarded an uncommitted tail) AND re-verified the
// data cleanly. It confirms the outcome; it is not an error. Detail text comes from settingsVm.
Popup {
    id: root

    modal:       true
    dim:         true
    focus:       true
    closePolicy: Popup.CloseOnEscape

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min((Overlay.overlay ? Overlay.overlay.width : 480) * 0.92, 460)

    onOpened: continueBtn.forceActiveFocus()

    Overlay.modal: Rectangle {
        color: Qt.rgba(Theme.color.canvas.r, Theme.color.canvas.g, Theme.color.canvas.b, 0.55)
    }

    background: Rectangle {
        radius:       Theme.radius.card
        color:        Theme.color.surface
        border.color: Theme.color.border
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Keys.onEscapePressed: root.close()

        RowLayout {
            Layout.topMargin:   Theme.space.lg
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            Layout.fillWidth:   true
            spacing: Theme.space.sm

            Text {
                text: "✓"
                color: Theme.color.income
                font.pixelSize: Theme.font.xl; font.weight: Theme.font.weightBold
                font.family: Theme.font.uiFamily
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Recovered successfully")
                color: Theme.color.textPrimary
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
                  : qsTr("Your accounting data was verified against its authoritative history and no "
                       + "inconsistencies were found. You can continue working normally.")
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
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
                id: continueBtn
                text: qsTr("Continue")
                variant: "primary"
                onClicked: root.close()
            }
        }
    }
}
