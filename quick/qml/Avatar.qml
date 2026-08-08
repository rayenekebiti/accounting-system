import QtQuick
import App

Item {
    id: root

    property string name: ""
    property int    diameter: 36

    implicitWidth:  diameter
    implicitHeight: diameter

    // Compute initials: first letter of first two space-separated words,
    // or first two characters of name if only one word.
    readonly property string _initials: {
        var parts = name.trim().split(/\s+/)
        var raw = ""
        if (parts.length >= 2)
            raw = parts[0].charAt(0) + parts[1].charAt(0)
        else if (name.length >= 2)
            raw = name.substring(0, 2)
        else
            raw = name
        return raw.toUpperCase()
    }

    // Pick a bg color deterministically from name char-code sum
    readonly property var _palette: [
        Theme.color.brand,
        Theme.color.accent,
        Theme.color.info,
        Theme.color.pending,
        Theme.color.income
    ]
    readonly property color _bg: {
        var sum = 0
        for (var i = 0; i < name.length; i++)
            sum += name.charCodeAt(i)
        return _palette[sum % _palette.length]
    }

    Rectangle {
        anchors.fill: parent
        radius: root.diameter / 2
        color: root._bg

        Text {
            anchors.centerIn: parent
            text:  root._initials
            color: Theme.color.textOnBrand
            font.pixelSize: Theme.font.sm
            font.weight: Theme.font.weightBold
            font.family: Theme.font.uiFamily
        }
    }
}
