import QtQuick
import App

Badge {
    id: root

    property string status: ""

    // Map status string → Badge tone
    readonly property string _tone: {
        switch (status) {
        case "Draft":   return "neutral"
        case "Posted":  return "info"
        case "Paid":    return "income"
        case "Overdue": return "pending"
        case "Void":    return "expense"
        default:        return "neutral"
        }
    }

    function statusLabel(s) {
        i18n.language   // dependency: re-run this binding on a live language switch (qsTr inside a
                        // function isn't caught by retranslate() unless the binding depends on the language)
        switch (s) {
        case "Draft":   return qsTr("Draft")
        case "Posted":  return qsTr("Posted")
        case "Paid":    return qsTr("Paid")
        case "Overdue": return qsTr("Overdue")
        case "Void":    return qsTr("Void")
        default:        return s
        }
    }

    text: root.statusLabel(root.status)
    tone: root._tone
}
