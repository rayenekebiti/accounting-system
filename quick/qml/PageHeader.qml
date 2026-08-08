import QtQuick
import QtQuick.Layouts
import App

RowLayout {
    id: root

    property string title:    ""
    property string subtitle: ""

    default property alias actions: actionsRow.data

    spacing: Theme.space.md

    ColumnLayout {
        spacing: Theme.space.xxs

        Text {
            text:  root.title
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.xxl
            font.weight: Theme.font.weightBold
            font.family: Theme.font.uiFamily
        }

        Text {
            visible: root.subtitle.length > 0
            text:  root.subtitle
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.sm
            font.family: Theme.font.uiFamily
        }
    }

    // Flexible spacer — pushes actions to the inline-end; mirrors correctly in RTL
    Item { Layout.fillWidth: true }

    RowLayout {
        id: actionsRow
        spacing: Theme.space.sm
    }
}
