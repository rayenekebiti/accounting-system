import QtQuick
import App

// 1px separator. Works inside Layouts: set Layout.fillWidth (horizontal) or
// Layout.fillHeight (vertical); the implicit size provides the 1px thickness.
Rectangle {
    id: root

    property string orientation: "horizontal"  // horizontal | vertical

    implicitWidth:  orientation === "vertical" ? 1 : 0
    implicitHeight: orientation === "vertical" ? 0 : 1

    color: Theme.color.border
}
