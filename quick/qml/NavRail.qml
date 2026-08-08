import QtQuick
import QtQuick.Layouts
import App

// NavRail — slim sidebar navigation rail.
// Placed via RowLayout in Main so it mirrors automatically in RTL (logical order).
// property string current: active screen key ("invoices" | "customers")
// signal navigate(string key): emitted when user taps an item.
Item {
    id: root

    property string current: "invoices"

    signal navigate(string key)

    implicitWidth: 180

    // Surface + inline-end border
    Rectangle {
        anchors.fill: parent
        color: Theme.color.surface
    }

    // 1px divider on the inline-end edge (logical: right in LTR, left in RTL)
    Rectangle {
        anchors.top:    parent.top
        anchors.bottom: parent.bottom
        anchors.right:  parent.right
        width: 1
        color: Theme.color.border
    }

    // Nav items
    Column {
        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    parent.top
        anchors.topMargin: Theme.space.lg
        spacing: Theme.space.xs

        Repeater {
            // Model is stable KEYS only (no qsTr) so a language switch does NOT
            // rebuild the delegates. Labels are qsTr in the delegate → they
            // retranslate in place (no object churn).
            model: ["invoices", "customers", "suppliers", "payments", "expenses", "ledger", "settings"]

            delegate: Item {
                id: navItem

                required property string modelData   // the nav key

                width:  parent.width
                height: 40

                readonly property bool   selected: root.current === modelData
                readonly property string label: modelData === "invoices" ? qsTr("Invoices")
                                                : modelData === "customers" ? qsTr("Customers")
                                                : modelData === "suppliers" ? qsTr("Suppliers")
                                                : modelData === "payments" ? qsTr("Payments")
                                                : modelData === "expenses" ? qsTr("Expenses")
                                                : modelData === "ledger" ? qsTr("Ledger")
                                                : qsTr("Settings")
                readonly property string glyph: modelData === "invoices" ? "🧾"
                                                : modelData === "customers" ? "👤"
                                                : modelData === "suppliers" ? "🏢"
                                                : modelData === "payments" ? "💵"
                                                : modelData === "expenses" ? "💸"
                                                : modelData === "ledger" ? "📒" : "⚙"
                property bool hovered: false

                // Keyboard + screen-reader navigation. Announced as a button; the
                // selected item adds a "current" hint so SR users know where they are.
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: navItem.label
                Accessible.description: navItem.selected ? qsTr("Current screen") : ""
                Accessible.focusable: true
                Accessible.onPressAction: root.navigate(navItem.modelData)
                Keys.onReturnPressed: root.navigate(navItem.modelData)
                Keys.onEnterPressed:  root.navigate(navItem.modelData)
                Keys.onSpacePressed:  root.navigate(navItem.modelData)

                Rectangle {
                    anchors.fill:        parent
                    anchors.leftMargin:  Theme.space.sm
                    anchors.rightMargin: Theme.space.sm
                    radius:              Theme.radius.md
                    color: navItem.selected
                           ? Theme.color.brandSubtle
                           : (navItem.hovered ? Theme.color.surfaceMuted : "transparent")

                    Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

                    // Keyboard focus ring.
                    Rectangle {
                        anchors.fill: parent
                        color: "transparent"
                        radius: parent.radius
                        border.color: Theme.color.focusRing
                        border.width: 2
                        visible: navItem.activeFocus
                    }
                }

                RowLayout {
                    anchors.fill:        parent
                    anchors.leftMargin:  Theme.space.md
                    anchors.rightMargin: Theme.space.md
                    spacing:             Theme.space.sm

                    Text {
                        text:           navItem.glyph
                        font.pixelSize: Theme.font.base
                        font.family:    Theme.font.uiFamily
                    }

                    Text {
                        text:           navItem.label
                        color:          navItem.selected ? Theme.color.brand : Theme.color.textSecondary
                        font.pixelSize: Theme.font.base
                        font.weight:    navItem.selected ? Theme.font.weightSemibold : Theme.font.weightRegular
                        font.family:    Theme.font.uiFamily
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered:  navItem.hovered = true
                    onExited:   navItem.hovered = false
                    onClicked:  root.navigate(navItem.modelData)
                }
            }
        }
    }
}
