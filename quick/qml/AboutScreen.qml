import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// AboutScreen — product identity, the read-only LICENSE status, and the UPDATE surface. Licensing
// and updating are handled entirely by the commercial (C2) layer, exposed here through `platform`.
// All display text is composed in QML with qsTr from the structured state (never the C++ status
// strings), so it stays fully translatable.
ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth
    ScrollBar.vertical.policy: ScrollBar.AsNeeded

    // Bubbles up to Main to (re)open the Early Access welcome notice.
    signal earlyAccessRequested()

    // License state KEY -> translated label + tone (key stays English for logic).
    function licenseLabel(s) {
        i18n.language
        return s === "Trial"    ? qsTr("Trial")
             : s === "Personal" ? qsTr("Personal")
             : s === "Business" ? qsTr("Business")
             : s === "Expired"  ? qsTr("Expired")
             : qsTr("Invalid")
    }
    function licenseTone(s) {
        return s === "Personal" || s === "Business" ? "income"
             : s === "Trial" ? "pending" : "expense"
    }

    ColumnLayout {
        width: root.availableWidth
        spacing: Theme.space.lg

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("About")
            subtitle: qsTr("Product information and status.")
        }

        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.xs
                Text {
                    text: qsTr("Occountant")
                    color: Theme.color.textPrimary
                    font.pixelSize: Theme.font.xl; font.weight: Theme.font.weightBold
                    font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("Version %1 (%2)").arg(platform.appVersion).arg(platform.channel)
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
                }
                Text {
                    text: qsTr("A deterministic, event-sourced accounting application. Every figure is "
                             + "reconstructable from an immutable history, so your books are always "
                             + "verifiable and safe against crashes.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                    Layout.topMargin: Theme.space.xs
                }
            }
        }

        // ── Licensing (read-only status + activation) ─────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm
                    Text {
                        text: qsTr("Licensing")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                        font.family: Theme.font.uiFamily
                    }
                    Badge { tone: root.licenseTone(platform.licenseState); text: root.licenseLabel(platform.licenseState) }
                    Item { Layout.fillWidth: true }
                }
                Text {
                    visible: platform.licenseValid && platform.licenseDaysRemaining > 0
                    text: platform.licenseInGrace
                          ? qsTr("In grace period — renew to keep using Occountant.")
                          : qsTr("%n day(s) remaining.", "", platform.licenseDaysRemaining)
                    color: platform.licenseInGrace ? Theme.color.pending : Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }
                Text {
                    visible: !platform.licenseValid
                    text: qsTr("Your license is not valid. Enter a license key below to activate Occountant.")
                    color: Theme.color.expense
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap; Layout.fillWidth: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space.xs
                    spacing: Theme.space.sm
                    AppTextField {
                        id: keyField
                        Layout.fillWidth: true
                        placeholder: qsTr("Paste your license key")
                    }
                    AppButton {
                        text: qsTr("Activate")
                        variant: "secondary"
                        accessibleName: qsTr("Activate license key")
                        onClicked: {
                            activateResult.accepted = platform.activateLicense(keyField.text)
                            activateResult.visible = true
                        }
                    }
                }
                Text {
                    id: activateResult
                    property bool accepted: false
                    visible: false
                    text: accepted ? qsTr("License activated.") : qsTr("That license key was not accepted.")
                    color: accepted ? Theme.color.income : Theme.color.expense
                    font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
                }
            }
        }

        // ── Updates (check / stage / apply-on-restart) ────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm
                    Text {
                        text: qsTr("Updates")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                        font.family: Theme.font.uiFamily
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("Check for updates")
                        variant: "ghost"; size: "sm"
                        onClicked: platform.checkForUpdates()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    // Honest about v1: the app downloads + verifies the update, but installation is
                    // done by running the installer (it does not self-install on restart). Errors are
                    // surfaced reassuringly (the current version and your books are never affected).
                    text: platform.updateState === "Error"
                          ? qsTr("We couldn't complete the update. Your current version and your data are unaffected — please try again later.")
                          : platform.updateStaged
                            ? qsTr("Update %1 is downloaded and verified. Run the Occountant installer to finish updating.").arg(platform.availableVersion)
                            : platform.updateAvailable
                              ? qsTr("Version %1 is available.").arg(platform.availableVersion)
                              : platform.updateState === "UpToDate"
                                ? qsTr("You are up to date.")
                                : qsTr("Updates are checked locally; nothing is installed while the app is running.")
                }
                RowLayout {
                    spacing: Theme.space.sm
                    visible: platform.updateAvailable || platform.updateStaged
                    AppButton {
                        visible: platform.updateAvailable && !platform.updateStaged
                        text: qsTr("Download & stage")
                        variant: "primary"; size: "sm"
                        onClicked: platform.downloadUpdate()
                    }
                    AppButton {
                        visible: platform.updateStaged
                        text: qsTr("Cancel staged update")
                        variant: "ghost"; size: "sm"
                        onClicked: platform.rollbackUpdate()
                    }
                }
            }
        }

        // ── Early Access Program ──────────────────────────────────────────────
        Card {
            Layout.fillWidth: true
            ColumnLayout {
                width: parent.width
                spacing: Theme.space.sm
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space.sm
                    Text {
                        text: qsTr("Early Access Program")
                        color: Theme.color.textPrimary
                        font.pixelSize: Theme.font.md; font.weight: Theme.font.weightSemibold
                        font.family: Theme.font.uiFamily
                    }
                    Item { Layout.fillWidth: true }
                    AppButton {
                        text: qsTr("View program details")
                        variant: "ghost"; size: "sm"
                        onClicked: root.earlyAccessRequested()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("You're using Occountant during Early Access — we improve it with feedback "
                             + "from real businesses. Your data always stays on your computer.")
                    color: Theme.color.textSecondary
                    font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
                    wrapMode: Text.WordWrap
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
