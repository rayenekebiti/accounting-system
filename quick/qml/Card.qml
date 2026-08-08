import QtQuick
import QtQuick.Effects
import App

// Rounded surface with a soft shadow. Sizes to its content: place a single
// layout as content and bind its width to `parent.width` so implicit height
// flows upward (a plain anchors.fill Item cannot derive height from children).
Item {
    id: root

    property int padding:   Theme.space.lg
    property int elevation: 1   // 1 or 2

    default property alias content: holder.data

    implicitWidth:  holder.childrenRect.width  + padding * 2
    implicitHeight: holder.childrenRect.height + padding * 2

    // Shadow layer (rendered behind the surface)
    Rectangle {
        id: shadowRect
        anchors.fill: bgRect
        anchors.margins: -1
        radius: bgRect.radius
        color: bgRect.color
        visible: false
    }

    MultiEffect {
        source: shadowRect
        anchors.fill: shadowRect
        shadowEnabled: true
        shadowColor:   Theme.elevation.shadowColor
        shadowOpacity: root.elevation === 2 ? Theme.elevation.e2Opacity : Theme.elevation.e1Opacity
        blurMax:       root.elevation === 2 ? Theme.elevation.e2Blur    : Theme.elevation.e1Blur
        shadowVerticalOffset: root.elevation === 2 ? Theme.elevation.e2Y : Theme.elevation.e1Y
        shadowHorizontalOffset: 0
    }

    Rectangle {
        id: bgRect
        anchors.fill: parent
        radius: Theme.radius.card
        color:  Theme.color.surface
        border.color: Theme.color.border
        border.width: 1
    }

    // Content holder: full width minus padding; height tracks children.
    Item {
        id: holder
        anchors.left:  parent.left
        anchors.right: parent.right
        anchors.top:   parent.top
        anchors.margins: root.padding
        height: childrenRect.height
    }
}
