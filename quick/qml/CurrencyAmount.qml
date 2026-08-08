import QtQuick
import App

// Displays a preformatted money string with an optional directional sign.
// Consumers set `amount` (NOT `text`): the inherited `text` is derived as
// `_prefix + amount`, so the +/− sign can never be accidentally overridden.
// Sign is shown as a glyph AND colour — never colour alone (accessibility).
Text {
    id: root

    property string amount: ""
    property string sign: "none"   // none | pos | neg

    readonly property string _prefix: {
        if (sign === "pos") return "+"
        if (sign === "neg") return "−"   // minus sign U+2212
        return ""
    }

    readonly property color _color: {
        if (sign === "pos") return Theme.color.income
        if (sign === "neg") return Theme.color.expense
        return Theme.color.textPrimary
    }

    text: _prefix + amount
    color: _color
    horizontalAlignment: Text.AlignRight  // money: trailing-aligned column (mirror-flips in RTL)
    font.pixelSize: Theme.font.md
    font.weight: Theme.font.weightBold
    font.family: Theme.font.numericFamily
    font.features: ({ "tnum": 1, "lnum": 1 })
}
