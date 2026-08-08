import QtQuick
import QtQuick.Layouts
import App

// A compact, start-aligned KPI cell for summary bars: a micro uppercase label,
// a prominent tabular value, and an optional small context sub-line. Values use
// the numeric font + tabular figures so they align column-to-column for scanning.
// Start-alignment (not centered) makes the bar mirror correctly in RTL.
Item {
    id: root

    property string label:    ""
    property string value:    ""
    property string sub:      ""
    property string tone:     ""      // "" | income | expense | pending -> value color
    property bool   emphasis: false   // hero sizing (larger value)

    readonly property color _valueColor: (tone.length > 0 && Theme.color[tone] !== undefined)
        ? Theme.color[tone]
        : Theme.color.textPrimary

    implicitHeight: col.implicitHeight

    ColumnLayout {
        id: col
        width: parent.width
        spacing: Theme.space.xxs

        // Micro label — uppercase, tracked, muted: the "production accounting" tell.
        Text {
            Layout.fillWidth: true
            text: root.label.toUpperCase()
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.weight: Theme.font.weightSemibold
            font.family: Theme.font.uiFamily
            font.letterSpacing: 0.5
            elide: Text.ElideRight
        }

        // Value — tabular numerals, size driven by emphasis (hero vs secondary).
        Text {
            Layout.fillWidth: true
            text: root.value
            color: root._valueColor
            font.pixelSize: root.emphasis ? Theme.font.xxl : Theme.font.lg
            font.weight: Theme.font.weightBold
            font.family: Theme.font.numericFamily
            font.features: ({ "tnum": 1, "lnum": 1 })
            elide: Text.ElideRight
        }

        // Optional context (count, status) — small and muted.
        Text {
            Layout.fillWidth: true
            visible: root.sub.length > 0
            text: root.sub
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs
            font.family: Theme.font.uiFamily
            elide: Text.ElideRight
        }
    }
}
