import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// OnboardingWizard — first-run company profile. A full-window overlay shown BEFORE the main app
// when `onboardingVm.needed` is true. It writes ONLY settings (via onboardingVm) — no accounting
// events — and hands off to the app on commit or explicit skip. Skippable only by the explicit
// "Skip for now" action.
Rectangle {
    id: root
    objectName: "onboardingWizard"
    anchors.fill: parent
    color: Theme.color.canvas
    visible: false

    signal finished()

    function start() { visible = true }

    Connections {
        target: onboardingVm
        function onFinished() {
            // Apply the chosen language immediately, then hand off.
            i18n.setLanguage(onboardingVm.language)
            root.visible = false
            root.finished()
        }
    }

    // Center the card.
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: card.implicitHeight + 80
        clip: true

        Card {
            id: card
            width: Math.min(560, parent.width - 48)
            anchors.horizontalCenter: parent.horizontalCenter
            y: 40

            ColumnLayout {
                width: parent.width
                spacing: Theme.space.md

                Text {
                    text: qsTr("Welcome to Occountant")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.xl
                    font.bold: true
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("Let's set up your business. You can change any of this later in Settings.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm
                    font.family: Theme.font.uiFamily
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }

                AppTextField {
                    objectName: "obBusinessName"
                    Layout.fillWidth: true
                    label: qsTr("Business name")
                    text: onboardingVm.businessName
                    onTextChanged: onboardingVm.businessName = text
                    error: onboardingVm.canComplete ? "" : qsTr("required")
                }
                AppTextField {
                    Layout.fillWidth: true
                    label: qsTr("Address")
                    text: onboardingVm.address
                    onTextChanged: onboardingVm.address = text
                }
                AppTextField {
                    Layout.fillWidth: true
                    label: qsTr("Tax number")
                    text: onboardingVm.taxNumber
                    onTextChanged: onboardingVm.taxNumber = text
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.md
                    AppTextField {
                        Layout.fillWidth: true
                        label: qsTr("Currency symbol")
                        text: onboardingVm.currency
                        onTextChanged: onboardingVm.currency = text
                    }
                    AppTextField {
                        Layout.fillWidth: true
                        label: qsTr("Fiscal year start (MM-DD)")
                        text: onboardingVm.fiscalYearStart
                        onTextChanged: onboardingVm.fiscalYearStart = text
                    }
                }

                // Language chooser (drives the app language + document defaults).
                Text {
                    text: qsTr("Language")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.xs
                    font.family: Theme.font.uiFamily
                }
                RowLayout {
                    spacing: Theme.space.sm
                    Repeater {
                        model: i18n.languages
                        AppButton {
                            required property var modelData
                            text: modelData.label
                            variant: onboardingVm.language === modelData.code ? "primary" : "secondary"
                            onClicked: onboardingVm.language = modelData.code
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space.md
                    AppButton {
                        objectName: "obSkip"
                        text: qsTr("Skip for now")
                        variant: "ghost"
                        onClicked: onboardingVm.skip()
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        objectName: "obFinish"
                        text: qsTr("Create company")
                        variant: "primary"
                        enabled: onboardingVm.canComplete
                        onClicked: onboardingVm.commit()
                    }
                }
            }
        }
    }
}
