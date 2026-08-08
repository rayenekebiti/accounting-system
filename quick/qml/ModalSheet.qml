import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// ModalSheet — card-style modal dialog shell.
//
// Footer slot approach: expose `property alias footerData: footerRow.data`
// so consumers assign a list of Items directly:
//
//   ModalSheet {
//       footerData: [
//           AppButton { text: "Cancel"; variant: "ghost"; onClicked: requestClose() },
//           AppButton { text: "Save";   variant: "primary"; onClicked: ... }
//       ]
//   }
//
// Body uses the QML `default property alias` so child items go into the body
// scroll area automatically:
//
//   ModalSheet {
//       SomeForm { ... }   // goes into body
//   }
//
// Signals requestClose() on Esc, scrim click, or close button.
// Close-guard logic lives in the consumer — ModalSheet never closes itself.

Popup {
    id: root

    property string title:    ""
    signal requestClose()

    // Initial keyboard focus target. Consumers that don't manage their own initial
    // focus set this to their first field so a keyboard/screen-reader user lands
    // inside the form on open rather than having to Tab in. modal:true already traps
    // focus within the dialog and Qt restores focus to the trigger on close, so we
    // only seed the entry point — and only when explicitly asked, so it never steals
    // focus from a consumer (e.g. the editors) that focuses its own first field.
    property Item initialFocusItem: null
    onOpened: if (initialFocusItem) initialFocusItem.forceActiveFocus()

    // default content → bodyCol
    default property alias content:    bodyCol.data
    // footer items → footerRow
    property alias         footerData: footerRow.data

    modal:       true
    dim:         true
    focus:       true   // capture key events (Esc) + allow child fields to take focus
    closePolicy: Popup.NoAutoClose

    // Center in the overlay
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay

    width:  Math.min((Overlay.overlay ? Overlay.overlay.width  : 980) * 0.92, 760)
    height: Math.min(implicitHeight, (Overlay.overlay ? Overlay.overlay.height : 700) * 0.9)

    // Scrim: calm canvas-tinted dim (Popup dim renders the Qt overlay; this
    // MouseArea catches clicks on the dim region — but since modal:true the
    // overlay blocks them. We use an Overlay background instead.)
    Overlay.modal: Rectangle {
        color: Qt.rgba(
            Theme.color.canvas.r,
            Theme.color.canvas.g,
            Theme.color.canvas.b,
            0.55)

        MouseArea {
            anchors.fill: parent
            onClicked:    root.requestClose()
        }
    }

    // ── Card background ───────────────────────────────────────────────────────
    background: Rectangle {
        radius:       Theme.radius.card
        color:        Theme.color.surface
        border.color: Theme.color.border
        border.width: 1
    }

    // ── Content ───────────────────────────────────────────────────────────────
    contentItem: ColumnLayout {
        spacing: 0

        // Esc → requestClose (must be on contentItem, an Item, not on Popup root)
        Keys.onEscapePressed: root.requestClose()

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing:          Theme.space.sm
            Layout.topMargin:    Theme.space.lg
            Layout.leftMargin:   Theme.space.xl
            Layout.rightMargin:  Theme.space.lg
            Layout.bottomMargin: Theme.space.md

            Text {
                Layout.fillWidth: true
                text:           root.title
                font.family:    Theme.font.uiFamily
                font.pixelSize: Theme.font.lg
                font.weight:    Theme.font.weightBold
                color:          Theme.color.textPrimary
                elide:          Text.ElideRight
            }

            IconButton {
                content: "✕"  // ✕
                accessibleName: qsTr("Close")
                onClicked: root.requestClose()
            }
        }

        // Divider
        Divider {}

        // Body (scrollable)
        ScrollView {
            id: bodyScroll
            Layout.fillWidth:  true
            Layout.fillHeight: true
            clip: true

            Item {
                id:             bodyPad
                width:          bodyScroll.availableWidth
                implicitHeight: bodyCol.implicitHeight + Theme.space.lg * 2

                ColumnLayout {
                    id:      bodyCol
                    x:       Theme.space.xl
                    y:       Theme.space.lg
                    width:   parent.width - Theme.space.xl * 2
                    spacing: Theme.space.md
                }
            }
        }

        // Divider
        Divider {}

        // Footer
        RowLayout {
            id: footerRow
            Layout.fillWidth:    true
            Layout.topMargin:    Theme.space.md
            Layout.bottomMargin: Theme.space.md
            Layout.leftMargin:   Theme.space.xl
            Layout.rightMargin:  Theme.space.xl
            spacing:             Theme.space.sm
        }
    }
}
