import QtQuick
import QtQuick.Controls.Basic
import App

Item {
    id: root

    property string text: ""
    property string variant: "primary"   // primary | secondary | ghost | danger
    property string size: "md"           // md | sm
    property bool   loading: false
    // Accessible label override — defaults to the visible text, but icon-only or
    // ambiguous buttons can set an explicit name (e.g. "Save invoice").
    property string accessibleName: text

    signal clicked()

    // Single activation path shared by mouse, Enter/Space, and screen-reader press.
    function activate() { if (!loading) root.clicked() }

    // ── Accessibility + keyboard ──────────────────────────────────────────────
    // Custom Item-based control: it must declare its own semantics and key handling
    // (a MouseArea gives neither). Tab-reachable, announced as a button, activated
    // by Enter/Space, with a visible focus ring.
    activeFocusOnTab: !loading
    Accessible.role: Accessible.Button
    Accessible.name: root.accessibleName
    Accessible.focusable: true
    Accessible.onPressAction: root.activate()
    Keys.onReturnPressed: root.activate()
    Keys.onEnterPressed:  root.activate()
    Keys.onSpacePressed:  root.activate()

    readonly property int _heightMd: 36
    readonly property int _heightSm: 28
    readonly property int _hPadMd:   Theme.space.lg
    readonly property int _hPadSm:   Theme.space.md

    implicitHeight: size === "sm" ? _heightSm : _heightMd
    implicitWidth:  label.implicitWidth + 2 * (size === "sm" ? _hPadSm : _hPadMd)

    // ── resolved colors ──────────────────────────────────────────────────────
    readonly property color _bgColor: {
        if (variant === "primary") return hover ? Theme.color.brandHover : Theme.color.brand
        if (variant === "secondary") return Theme.color.surface
        if (variant === "ghost")  return "transparent"
        if (variant === "danger") return Theme.color.expense
        return Theme.color.brand
    }
    readonly property color _textColor: {
        if (variant === "secondary") return Theme.color.textPrimary
        if (variant === "ghost")     return Theme.color.brand
        return Theme.color.textOnBrand
    }
    readonly property bool hover: mouseArea.containsMouse && !loading

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: Theme.radius.md
        color: root._bgColor
        border.color: root.variant === "secondary" ? Theme.color.border : "transparent"
        border.width: root.variant === "secondary" ? 1 : 0
        opacity: loading ? 0.6 : 1.0

        Behavior on color { ColorAnimation { duration: Theme.motion.fast } }

        // Keyboard focus ring — visible only under keyboard focus, so mouse users
        // never see it but keyboard operators always know where they are.
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: Theme.radius.md + 3
            color: "transparent"
            border.color: Theme.color.focusRing
            border.width: 2
            visible: root.activeFocus
        }

        // Loading indicator
        BusyIndicator {
            anchors.centerIn: parent
            running: loading
            visible: loading
            width:   root.size === "sm" ? 16 : 20
            height:  width
            palette.dark: root._textColor
        }

        // Label
        Text {
            id: label
            anchors.centerIn: parent
            visible: !loading
            text: root.text
            color: root._textColor
            font.pixelSize: root.size === "sm" ? Theme.font.sm : Theme.font.base
            font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: !loading
        cursorShape: Qt.PointingHandCursor
        onClicked: root.activate()
    }
}
