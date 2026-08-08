import QtQuick
import QtQuick.Layouts
import App

Card {
    id: root

    property string title:   ""
    property var    value:   ""
    property string tone:    ""    // optional; e.g. "income", "expense", "pending"
    property string caption: ""   // optional

    // Resolve value color: tone-keyed if provided, else textPrimary
    readonly property color _valueColor: (tone.length > 0 && Theme.color[tone] !== undefined)
        ? Theme.color[tone]
        : Theme.color.textPrimary

    padding: Theme.space.lg

    ColumnLayout {
        width: parent.width
        spacing: Theme.space.xs

        Text {
            Layout.alignment: Qt.AlignHCenter
            text:  String(root.value)
            color: root._valueColor
            font.pixelSize: Theme.font.xxxl
            font.weight: Theme.font.weightBold
            font.family: Theme.font.uiFamily
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text:  root.title
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.base
            font.family: Theme.font.uiFamily
        }

        Text {
            visible: root.caption.length > 0
            Layout.alignment: Qt.AlignHCenter
            text:  root.caption
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm
            font.family: Theme.font.uiFamily
        }
    }
}
