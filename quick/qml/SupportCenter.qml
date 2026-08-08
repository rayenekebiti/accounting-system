import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// SupportCenter — Settings → Support Center. Report a problem (stored locally as a ticket), attach a
// privacy-safe diagnostics bundle (app health only — never accounting data), see the stable Support
// ID, and track reports. Everything is local; nothing is transmitted. Bound to supportVm.
ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    // Category/severity: stable KEYS for storage, translated LABELS for display.
    readonly property var categoryKeys: ["Accounting","Invoice","Payment","Expense","Tax","Reports",
                                         "Backup","Update","Translation","Interface","Feature request","Other"]
    property var categoryOptions: [
        { value: 0,  label: qsTr("Accounting") },      { value: 1,  label: qsTr("Invoice") },
        { value: 2,  label: qsTr("Payment") },         { value: 3,  label: qsTr("Expense") },
        { value: 4,  label: qsTr("Tax") },             { value: 5,  label: qsTr("Reports") },
        { value: 6,  label: qsTr("Backup") },          { value: 7,  label: qsTr("Update") },
        { value: 8,  label: qsTr("Translation") },     { value: 9,  label: qsTr("Interface") },
        { value: 10, label: qsTr("Feature request") }, { value: 11, label: qsTr("Other") }
    ]
    readonly property var severityKeys: ["Blocking","Important","Minor","Suggestion"]
    property var severityOptions: [
        { value: 0, label: qsTr("Blocking") },  { value: 1, label: qsTr("Important") },
        { value: 2, label: qsTr("Minor") },     { value: 3, label: qsTr("Suggestion") }
    ]

    property int  categoryIndex: 0
    property int  severityIndex: 2   // default: Minor
    property bool attach: true

    // A localized status label from a stored key.
    function statusLabel(k) {
        i18n.language   // dependency: retranslate on a live language switch (see StatusBadge)
        if (k === "Reviewing") return qsTr("Reviewing")
        if (k === "Confirmed") return qsTr("Confirmed")
        if (k === "Fixed")     return qsTr("Fixed")
        if (k === "Released")  return qsTr("Released")
        return qsTr("Received")
    }

    // Reusable labeled multi-line input.
    component LabeledArea: ColumnLayout {
        property alias label: lbl.text
        property alias text:  area.text
        property alias placeholder: area.placeholderText
        spacing: Theme.space.xs
        Layout.fillWidth: true
        Text {
            id: lbl
            font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
            color: Theme.color.textSecondary
        }
        TextArea {
            id: area
            Layout.fillWidth: true
            wrapMode: TextArea.Wrap
            font.family: Theme.font.uiFamily; font.pixelSize: Theme.font.base
            color: Theme.color.textPrimary
            placeholderTextColor: Theme.color.textSecondary
            leftPadding: Theme.space.md; rightPadding: Theme.space.md
            topPadding: Theme.space.sm;   bottomPadding: Theme.space.sm
            background: Rectangle {
                implicitHeight: 64
                radius: Theme.radius.md; color: Theme.color.surface
                border.color: area.activeFocus ? Theme.color.focusRing : Theme.color.border
                border.width: area.activeFocus ? 2 : 1
            }
        }
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.space.lg

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Support Center")
            subtitle: qsTr("Report a problem or share an idea. Everything stays on your computer.")
        }

        // ── Support ID + diagnostics ──────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md
                Text {
                    text: qsTr("Your Support ID")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Quote this ID when you contact us. It's a random label — not your name, "
                             + "and not tied to your machine.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    TextEdit {
                        id: idField
                        text: supportVm.supportId
                        readOnly: true; selectByMouse: true
                        Accessible.name: qsTr("Support ID")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.base; font.weight: Theme.font.weightSemibold
                        font.family: Theme.font.uiFamily
                    }
                    AppButton {
                        text: qsTr("Copy"); variant: "ghost"; size: "sm"
                        accessibleName: qsTr("Copy the support ID")
                        onClicked: { idField.selectAll(); idField.copy(); idField.deselect() }
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Create diagnostics bundle")
                        variant: "secondary"; loading: supportVm.busy
                        accessibleName: qsTr("Create a diagnostics bundle to send to support")
                        onClicked: supportVm.exportDiagnostics()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: supportVm.lastBundlePath.length > 0
                    text: qsTr("Saved to: %1").arg(supportVm.lastBundlePath)
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap
                }
            }
        }

        // ── Report a problem ──────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Report a problem")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    Select {
                        Layout.fillWidth: true
                        label: qsTr("Category"); model: root.categoryOptions
                        currentValue: root.categoryIndex
                        onActivated: (v) => root.categoryIndex = v
                    }
                    Select {
                        Layout.fillWidth: true
                        label: qsTr("Severity"); model: root.severityOptions
                        currentValue: root.severityIndex
                        onActivated: (v) => root.severityIndex = v
                    }
                }

                LabeledArea {
                    id: whatField
                    label: qsTr("What happened?")
                    placeholder: qsTr("Describe what you saw.")
                }
                LabeledArea {
                    id: expectedField
                    label: qsTr("What did you expect?")
                    placeholder: qsTr("Describe what you expected instead.")
                }
                LabeledArea {
                    id: stepsField
                    label: qsTr("Steps to reproduce")
                    placeholder: qsTr("List the steps, if you can.")
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm
                    CheckBox {
                        id: attachBox
                        checked: root.attach
                        onToggled: root.attach = checked
                    }
                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Attach diagnostics (app health only — never your accounting data)")
                        color: Theme.color.textSecondary
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    AppButton {
                        text: qsTr("Submit report")
                        variant: "primary"; loading: supportVm.busy
                        onClicked: {
                            supportVm.submitReport(root.categoryKeys[root.categoryIndex],
                                                   root.severityKeys[root.severityIndex],
                                                   whatField.text, expectedField.text, stepsField.text,
                                                   root.attach)
                            whatField.text = ""; expectedField.text = ""; stepsField.text = ""
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: supportVm.lastResult.length > 0
                        text: supportVm.lastResult === "error"
                              ? qsTr("Couldn't save your report. Please check your disk space and try again.")
                              : qsTr("Report saved: %1. Thank you — this helps us improve Occountant.")
                                  .arg(supportVm.lastResult)
                        color: supportVm.lastResult === "error" ? Theme.color.expense : Theme.color.income
                        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        // ── Your reports ──────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            visible: supportVm.tickets.length > 0
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm

                Text {
                    text: qsTr("Your reports")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }

                Repeater {
                    model: supportVm.tickets
                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        implicitWidth: parent.width
                        implicitHeight: ticketRow.implicitHeight + Theme.space.md * 2
                        radius: Theme.radius.md
                        color: Theme.color.surface
                        border.color: Theme.color.border; border.width: 1

                        RowLayout {
                            id: ticketRow
                            anchors.fill: parent
                            anchors.margins: Theme.space.md
                            spacing: Theme.space.md

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space.xxs
                                Text {
                                    text: modelData.id + " · " + modelData.createdIso
                                    color: Theme.color.textSecondary
                                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: (modelData.whatHappened && modelData.whatHappened.length > 0)
                                          ? modelData.whatHappened : qsTr("(no description)")
                                    color: Theme.color.textPrimary
                                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                                    wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                                }
                                Text {
                                    // Fully translated — the internal reward-eligibility note (which is
                                    // English operator metadata) is intentionally NOT surfaced to the user.
                                    visible: modelData.valuable === true
                                    text: qsTr("★ Your feedback was marked valuable — thank you.")
                                    color: Theme.color.income
                                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                                    wrapMode: Text.WordWrap
                                }
                            }

                            Badge { tone: "info"; text: root.statusLabel(modelData.status) }
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
