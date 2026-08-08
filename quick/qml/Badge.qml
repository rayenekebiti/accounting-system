import QtQuick
import App

Rectangle {
    id: root

    property string text: ""
    property string tone: "neutral"   // neutral | brand | income | expense | pending | info

    readonly property color _bg: {
        switch (tone) {
        case "brand":   return Theme.color.brandSubtle
        case "income":  return Theme.color.incomeSubtle
        case "expense": return Theme.color.expenseSubtle
        case "pending": return Theme.color.pendingSubtle
        case "info":    return Theme.color.infoSubtle
        default:        return Theme.color.surfaceMuted   // neutral
        }
    }
    readonly property color _fg: {
        switch (tone) {
        case "brand":   return Theme.color.brand
        case "income":  return Theme.color.income
        case "expense": return Theme.color.expense
        case "pending": return Theme.color.pending
        case "info":    return Theme.color.info
        default:        return Theme.color.textSecondary  // neutral
        }
    }

    implicitWidth:  lbl.implicitWidth  + Theme.space.md * 2
    implicitHeight: lbl.implicitHeight + Theme.space.xs * 2

    radius: Theme.radius.pill
    color:  _bg

    Text {
        id: lbl
        anchors.centerIn: parent
        text:  root.text
        color: root._fg
        font.pixelSize: Theme.font.xs
        font.weight: Theme.font.weightSemibold
        font.family: Theme.font.uiFamily
    }
}
