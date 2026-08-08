import QtQuick
import QtQuick.Layouts
import App

Item {
    id: root

    property string title:      qsTr("Nothing here yet")
    property string description: ""
    property string actionText: ""
    property string icon:       ""

    signal actionClicked()

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Theme.space.md

        // Optional icon glyph
        Text {
            visible: root.icon.length > 0
            Layout.alignment: Qt.AlignHCenter
            text:  root.icon
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xxxl
            font.family: Theme.font.uiFamily
        }

        // Title
        Text {
            Layout.alignment: Qt.AlignHCenter
            text:  root.title
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.lg
            font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily
        }

        // Description
        Text {
            visible: root.description.length > 0
            Layout.alignment: Qt.AlignHCenter
            text:  root.description
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family: Theme.font.uiFamily
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            lineHeight: Theme.rtl ? Theme.font.lineHeightArabic : Theme.font.lineHeightLatin
            lineHeightMode: Text.ProportionalHeight
        }

        // Optional action button — when there is no action it is hidden AND removed from the
        // accessibility tree, so a screen reader never encounters an unnamed, non-functional button.
        AppButton {
            visible: root.actionText.length > 0
            Accessible.ignored: root.actionText.length === 0
            Layout.alignment: Qt.AlignHCenter
            text: root.actionText
            onClicked: root.actionClicked()
        }
    }
}
