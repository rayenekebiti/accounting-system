import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// Select — token-styled labeled ComboBox.
// model: list of {value: int, label: string}
// currentValue: currently selected value (int). -1 = none.
// Emits activated(value) when user picks an item.
ColumnLayout {
    id: root

    property string     label:        ""
    property bool       required:     false
    property var        model:        []
    property int        currentValue: -1
    property string     error:        ""
    property string     placeholder:  ""

    signal activated(int value)

    spacing: Theme.space.xs

    // ── Label ─────────────────────────────────────────────────────────────────
    Text {
        visible: root.label.length > 0
        font.pixelSize: Theme.font.sm
        font.family:    Theme.font.uiFamily
        color:          Theme.color.textSecondary
        text: root.required
              ? root.label + " <font color='" + Theme.color.expense + "'>*</font>"
              : root.label
        textFormat: Text.RichText
    }

    // ── ComboBox ──────────────────────────────────────────────────────────────
    ComboBox {
        id: combo
        Layout.fillWidth: true

        model: root.model
        textRole:  "label"
        valueRole: "value"

        // Sync currentIndex → currentValue
        onActivated: {
            const v = combo.currentValue
            root.currentValue = v
            root.activated(v)
        }

        // When currentValue is set externally, find matching index
        Component.onCompleted: syncIndex()
        onModelChanged:        Qt.callLater(syncIndex)

        function syncIndex() {
            if (!root.model) return
            for (let i = 0; i < root.model.length; ++i) {
                if (root.model[i]["value"] === root.currentValue) {
                    combo.currentIndex = i
                    return
                }
            }
            combo.currentIndex = -1
        }

        Connections {
            target: root
            function onCurrentValueChanged() { combo.syncIndex() }
        }

        // ── Content item (displayed text) ─────────────────────────────────────
        contentItem: Text {
            leftPadding:    Theme.space.md
            rightPadding:   Theme.space.md + combo.indicator.width
            text:           combo.currentIndex < 0 ? root.placeholder : combo.displayText
            font.family:    Theme.font.uiFamily
            font.pixelSize: Theme.font.base
            color:          combo.currentIndex < 0
                                ? Theme.color.textSecondary
                                : Theme.color.textPrimary
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        // ── Background (match FieldInput) ─────────────────────────────────────
        background: Rectangle {
            implicitHeight: 36
            radius:       Theme.radius.md
            color:        Theme.color.surface
            border.color: root.error.length > 0
                              ? Theme.color.expense
                              : (combo.activeFocus ? Theme.color.focusRing : Theme.color.border)
            border.width: combo.activeFocus ? 2 : 1

            Behavior on border.color { ColorAnimation { duration: Theme.motion.fast } }
        }

        // ── Popup ─────────────────────────────────────────────────────────────
        popup: Popup {
            y:      combo.height + Theme.space.xxs
            width:  combo.width
            implicitHeight: contentItem.implicitHeight
            padding: Theme.space.xxs

            background: Rectangle {
                radius:       Theme.radius.md
                color:        Theme.color.surface
                border.color: Theme.color.border
                border.width: 1
            }

            contentItem: ListView {
                clip:          true
                implicitHeight: Math.min(contentHeight, 240)
                model:         combo.popup.visible ? combo.delegateModel : null
                currentIndex:  combo.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }

        // ── Delegate ──────────────────────────────────────────────────────────
        delegate: ItemDelegate {
            width: combo.width
            highlighted: combo.highlightedIndex === index

            contentItem: Text {
                text:           modelData["label"] ?? ""
                font.family:    Theme.font.uiFamily
                font.pixelSize: Theme.font.base
                color:          highlighted ? Theme.color.brand : Theme.color.textPrimary
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                color: highlighted ? Theme.color.brandSubtle : "transparent"
                radius: Theme.radius.sm
            }
        }
    }

    // ── Error ─────────────────────────────────────────────────────────────────
    Text {
        visible:          root.error.length > 0
        text:             root.error
        color:            Theme.color.expense
        font.pixelSize:   Theme.font.xs
        font.family:      Theme.font.uiFamily
        wrapMode:         Text.WordWrap
        Layout.fillWidth: true
    }
}
