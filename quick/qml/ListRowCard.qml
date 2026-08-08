import QtQuick
import App

// A clickable, hover-highlighted row container whose HEIGHT is derived from its
// content. Consumers place a single layout as content and bind its width to
// `parent.width` (NOT anchors.fill) so implicit height can flow upward:
//
//   ListRowCard {
//       width: ListView.view.width
//       RowLayout { width: parent.width; ... }
//   }
//
Item {
    id: root

    default property alias content: holder.data
    property int padding: Theme.space.lg

    signal clicked()

    implicitWidth: 240
    implicitHeight: holder.childrenRect.height + padding * 2

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radius.md
        color: mouseArea.containsMouse ? Theme.color.surfaceMuted : Theme.color.surface
        border.color: Theme.color.border
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }
    }

    // Content holder: full width minus padding; height tracks its children so
    // the row sizes to its content instead of collapsing.
    Item {
        id: holder
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.top:   parent.top
        anchors.margins: root.padding
        height: childrenRect.height
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
