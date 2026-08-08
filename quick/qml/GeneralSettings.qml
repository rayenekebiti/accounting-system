import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// GeneralSettings — display preferences (language, date format, currency symbol).
// All values are machine-global QSettings prefs (settingsVm / i18n); none touch the store.
ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.space.lg

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("General")
            subtitle: qsTr("Language, date, and currency display preferences.")
        }

        // ── Language ──────────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm

                Text {
                    text: qsTr("Language")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("Choose the language used throughout the application. Arabic switches the layout to right-to-left.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RowLayout {
                    spacing: Theme.space.sm
                    Repeater {
                        model: i18n.languages   // [{code,label,rtl}, …]
                        delegate: Chip {
                            required property var modelData
                            text: modelData.label
                            selected: i18n.language === modelData.code
                            onClicked: i18n.setLanguage(modelData.code)
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ── Date format ───────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm

                Text {
                    text: qsTr("Date format")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("How dates are displayed. Today shown as a preview.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                RowLayout {
                    spacing: Theme.space.sm
                    Repeater {
                        model: ["yyyy-MM-dd", "dd/MM/yyyy", "MM/dd/yyyy", "d MMM yyyy"]
                        delegate: Chip {
                            required property string modelData
                            text: Qt.formatDate(new Date(), modelData)
                            selected: settingsVm.dateFormat === modelData
                            onClicked: settingsVm.dateFormat = modelData
                        }
                    }
                    Item { Layout.fillWidth: true }
                }
            }
        }

        // ── Currency ──────────────────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm

                Text {
                    text: qsTr("Currency symbol")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("The symbol shown next to monetary amounts. Amounts themselves are always stored exactly, in cents.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                AppTextField {
                    id: currencyField
                    Layout.preferredWidth: 160
                    placeholder: "$"
                    text: settingsVm.currencySymbol
                    onEditingFinished: settingsVm.currencySymbol = currencyField.text
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
