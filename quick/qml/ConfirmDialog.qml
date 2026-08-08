import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// ConfirmDialog — compact modal for two-button confirmation.
// Signals confirmed() and cancelled().
// Esc → cancelled().
Popup {
    id: root

    property string title:          ""
    property string message:        ""
    property string confirmText:    qsTr("Confirm")
    property string cancelText:     qsTr("Cancel")
    property string confirmVariant: "danger"   // danger | primary

    signal confirmed()
    signal cancelled()

    modal:       true
    dim:         true
    focus:       true   // REQUIRED for Keys (Esc) + button focus to work in the popup
    closePolicy: Popup.NoAutoClose

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay

    // Seed focus on the SAFE choice: for a destructive action default to Cancel so
    // a reflexive Enter never confirms a delete; otherwise default to Confirm.
    onOpened: (confirmVariant === "danger" ? cancelBtn : confirmBtn).forceActiveFocus()

    width: Math.min((Overlay.overlay ? Overlay.overlay.width : 480) * 0.92, 440)

    Overlay.modal: Rectangle {
        color: Qt.rgba(
            Theme.color.canvas.r,
            Theme.color.canvas.g,
            Theme.color.canvas.b,
            0.55)
    }

    background: Rectangle {
        radius:       Theme.radius.card
        color:        Theme.color.surface
        border.color: Theme.color.border
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        // Esc → cancelled + close (on contentItem, an Item, not on Popup root)
        Keys.onEscapePressed: {
            root.cancelled()
            root.close()
        }

        // Title
        Text {
            Layout.fillWidth:   true
            Layout.topMargin:   Theme.space.lg
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            text:           root.title
            font.family:    Theme.font.uiFamily
            font.pixelSize: Theme.font.lg
            font.weight:    Theme.font.weightBold
            color:          Theme.color.textPrimary
            wrapMode:       Text.WordWrap
        }

        // Message
        Text {
            visible:            root.message.length > 0
            Layout.fillWidth:   true
            Layout.leftMargin:  Theme.space.xl
            Layout.rightMargin: Theme.space.xl
            text:           root.message
            font.family:    Theme.font.uiFamily
            font.pixelSize: Theme.font.base
            color:          Theme.color.textSecondary
            wrapMode:       Text.WordWrap
        }

        // Divider
        Divider {}

        // Footer buttons
        RowLayout {
            Layout.fillWidth:    true
            Layout.bottomMargin: Theme.space.lg
            Layout.leftMargin:   Theme.space.xl
            Layout.rightMargin:  Theme.space.xl
            spacing:             Theme.space.sm

            Item { Layout.fillWidth: true }

            AppButton {
                id:      cancelBtn
                text:    root.cancelText
                variant: "ghost"
                onClicked: {
                    root.cancelled()
                    root.close()
                }
            }

            AppButton {
                id:      confirmBtn
                text:    root.confirmText
                variant: root.confirmVariant
                onClicked: {
                    root.confirmed()
                    root.close()
                }
            }
        }
    }
}
