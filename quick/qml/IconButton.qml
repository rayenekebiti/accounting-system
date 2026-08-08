import QtQuick
import App

Item {
    id: root

    property string content: ""
    property string variant: "ghost"
    // Icon-only: the glyph is meaningless to a screen reader, so an accessibleName
    // is REQUIRED for any non-decorative icon button (e.g. "Close", "Remove line").
    property string accessibleName: ""

    signal clicked()

    readonly property int _size: 32

    implicitWidth:  _size
    implicitHeight: _size

    // ── Accessibility + keyboard (see AppButton for the reference pattern) ─────
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: root.accessibleName
    Accessible.focusable: true
    Accessible.onPressAction: root.clicked()
    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed:  root.clicked()
    Keys.onSpacePressed:  root.clicked()

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius.md
        color: mouseArea.containsMouse ? Theme.color.surfaceMuted : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

        // Keyboard focus ring.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: Theme.radius.md + 2
            color: "transparent"
            border.color: Theme.color.focusRing
            border.width: 2
            visible: root.activeFocus
        }

        Text {
            anchors.centerIn: parent
            text: root.content
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family: Theme.font.uiFamily
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
