import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// CompanySettings — the business's own details (name, address, tax number, email).
// Stored as machine-global QSettings prefs (settingsVm); used for display and, later, on
// printed/exported documents. Never touches the accounting store.
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
            title:    qsTr("Company")
            subtitle: qsTr("Your business details, shown on documents and exports.")
        }

        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                AppTextField {
                    id: nameField
                    Layout.fillWidth: true
                    label:       qsTr("Business name")
                    placeholder: qsTr("Acme Trading Co.")
                    text:        settingsVm.companyName
                    onEditingFinished: settingsVm.companyName = nameField.text
                }
                AppTextField {
                    id: addressField
                    Layout.fillWidth: true
                    label:       qsTr("Address")
                    placeholder: qsTr("123 Market Street, Suite 400")
                    text:        settingsVm.companyAddress
                    onEditingFinished: settingsVm.companyAddress = addressField.text
                }
                AppTextField {
                    id: taxIdField
                    Layout.fillWidth: true
                    label:       qsTr("Tax registration number")
                    placeholder: qsTr("e.g. VAT / GST number")
                    text:        settingsVm.companyTaxId
                    onEditingFinished: settingsVm.companyTaxId = taxIdField.text
                }
                AppTextField {
                    id: emailField
                    Layout.fillWidth: true
                    label:            qsTr("Email")
                    placeholder:      qsTr("billing@example.com")
                    inputMethodHints: Qt.ImhEmailCharactersOnly
                    text:             settingsVm.companyEmail
                    onEditingFinished: settingsVm.companyEmail = emailField.text
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Changes are saved automatically as you leave each field.")
            color: Theme.color.textSecondary
            font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
        }

        Item { Layout.fillHeight: true }
    }
}
