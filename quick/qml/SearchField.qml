import QtQuick
import QtQuick.Controls.Basic
import App

Item {
    id: root

    property string placeholder: qsTr("Search…")
    property alias  text:        field.text

    implicitWidth:  280
    implicitHeight: field.implicitHeight

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color:  Theme.color.surface
        border.color: field.activeFocus ? Theme.color.focusRing : Theme.color.border
        border.width: field.activeFocus ? 2 : 1

        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }

        // Search glyph
        Text {
            id: searchGlyph
            anchors.left:           parent.left
            anchors.leftMargin:     Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            text:  "⌕"    // ⌕
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family: Theme.font.uiFamily
        }

        TextField {
            id: field
            anchors.left:   searchGlyph.right
            anchors.leftMargin:  Theme.space.xs
            anchors.right:  parent.right
            anchors.rightMargin: Theme.space.md
            anchors.verticalCenter: parent.verticalCenter
            height: parent.height

            placeholderText: root.placeholder
            // placeholderText is not exposed as the accessible name, so set it
            // explicitly — otherwise a screen reader announces an unnamed text field.
            Accessible.name: root.placeholder
            Accessible.searchEdit: true
            font.pixelSize:  Theme.font.base
            font.family:     Theme.font.sans
            background: Item {}     // transparent — outer Rectangle provides border
            leftPadding:  0
            rightPadding: 0
            topPadding:   Theme.space.sm
            bottomPadding: Theme.space.sm
        }
    }
}
