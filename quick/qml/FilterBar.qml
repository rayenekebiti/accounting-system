import QtQuick
import QtQuick.Layouts
import App

RowLayout {
    id: root

    property alias searchText:        search.text
    property alias searchPlaceholder: search.placeholder

    default property alias content: contentRow.data

    spacing: Theme.space.md

    SearchField {
        id: search
        Layout.preferredWidth: 240
    }

    Row {
        id: contentRow
        spacing: Theme.space.sm
    }
}
