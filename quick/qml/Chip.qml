import QtQuick
import App

Rectangle {
    id: root

    property string text:     ""
    property bool   selected: false

    signal clicked()

    implicitWidth:  lbl.implicitWidth  + Theme.space.lg * 2
    implicitHeight: lbl.implicitHeight + Theme.space.xs * 2

    radius: Theme.radius.pill
    color: {
        if (selected) return Theme.color.brand
        return mouseArea.containsMouse ? Theme.color.borderStrong : Theme.color.surfaceMuted
    }

    Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

    Text {
        id: lbl
        anchors.centerIn: parent
        text:  root.text
        color: root.selected ? Theme.color.textOnBrand : Theme.color.textSecondary
        font.pixelSize: Theme.font.sm
        font.weight: Theme.font.weightMedium
        font.family: Theme.font.uiFamily
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
