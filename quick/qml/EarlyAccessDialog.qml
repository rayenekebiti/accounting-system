import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// EarlyAccessDialog — the Early Access Program welcome notice. Shown automatically on first launch
// and after a major version update (driven by earlyAccessVm.shouldShow), and always reachable
// manually from the About screen. Professional and reassuring — it explains the program and the
// privacy guarantees without implying the software is unfinished or unsafe. Closing via Esc/scrim is
// treated as "remind me later" (non-committal). Fully localized (EN/FR/AR) + RTL via the shared shell.
ModalSheet {
    id: root
    objectName: "earlyAccessDialog"

    title: qsTr("Occountant Early Access Program")

    // Esc / scrim = remind me later (don't suppress, don't acknowledge).
    onRequestClose: { earlyAccessVm.remindLater(); root.close() }

    Text {
        Layout.fillWidth: true
        text: qsTr("Welcome — you're among the first businesses using Occountant. During Early Access "
                 + "we refine the product together with real users like you.")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Everything is fully functional and safe to use for your real accounting. Your books "
                 + "are kept with the same care as any released version.")
        color: Theme.color.textPrimary
        font.pixelSize: Theme.font.base; font.family: Theme.font.uiFamily
        wrapMode: Text.WordWrap
    }

    // Privacy reassurance card — the core promise, stated plainly (no fear framing).
    Rectangle {
        Layout.fillWidth: true
        radius: Theme.radius.md
        color:  Theme.color.brandSubtle
        implicitHeight: privacyText.implicitHeight + Theme.space.md * 2
        Text {
            id: privacyText
            anchors.fill: parent
            anchors.margins: Theme.space.md
            text: qsTr("Your data stays private. Occountant runs entirely on your computer — nothing is "
                     + "uploaded, no usage is tracked, and your financial data never leaves this machine "
                     + "unless you choose to export it.")
            color: Theme.color.textPrimary
            font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
            wrapMode: Text.WordWrap; verticalAlignment: Text.AlignVCenter
        }
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Your feedback directly shapes Occountant. You can report a problem or share an idea "
                 + "anytime from Settings → Support Center.")
        color: Theme.color.textSecondary
        font.pixelSize: Theme.font.sm; font.family: Theme.font.uiFamily
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        text: qsTr("Version %1 · Early Access").arg(earlyAccessVm.appVersion)
        color: Theme.color.textSecondary
        font.pixelSize: Theme.font.xs; font.family: Theme.font.uiFamily
    }

    footerData: [
        AppButton {
            text:    qsTr("Don't show again")
            variant: "ghost"
            onClicked: { earlyAccessVm.dontShowAgain(); root.close() }
        },
        Item { Layout.fillWidth: true },
        AppButton {
            text:    qsTr("Remind me later")
            variant: "secondary"
            onClicked: { earlyAccessVm.remindLater(); root.close() }
        },
        AppButton {
            text:    qsTr("Continue")
            variant: "primary"
            onClicked: { earlyAccessVm.acknowledge(); root.close() }
        }
    ]
}
