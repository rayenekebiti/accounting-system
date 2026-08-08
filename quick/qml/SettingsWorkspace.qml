import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// SettingsWorkspace — the "Settings & System" area, grouped under one nav item.
// A segmented tab bar switches between General preferences, Company details, Backup &
// Restore, read-only Diagnostics, and About. Every value is a machine-global preference
// persisted to QSettings (via settingsVm) or a read-only view of the engine (diagnosticsVm /
// backupVm) — nothing here mutates the accounting store. `activeTab` is drivable externally
// (screenshot harness) via objectName.
Item {
    id: root
    objectName: "settingsWorkspace"

    // "general" | "company" | "backup" | "support" | "diagnostics" | "about"
    property string activeTab: "general"

    // Bubbles to Main to (re)open the Early Access notice (from the About screen).
    signal earlyAccessRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space.lg
        spacing: Theme.space.md

        // ── Segmented tab bar ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            Chip {
                text: qsTr("General");     selected: root.activeTab === "general"
                onClicked: root.activeTab = "general"
            }
            Chip {
                text: qsTr("Company");     selected: root.activeTab === "company"
                onClicked: root.activeTab = "company"
            }
            Chip {
                text: qsTr("Backup");      selected: root.activeTab === "backup"
                onClicked: { root.activeTab = "backup"; backupVm.refresh() }
            }
            Chip {
                text: qsTr("Support");     selected: root.activeTab === "support"
                onClicked: { root.activeTab = "support"; supportVm.refresh() }
            }
            Chip {
                text: qsTr("Diagnostics"); selected: root.activeTab === "diagnostics"
                onClicked: { root.activeTab = "diagnostics"; diagnosticsVm.refresh() }
            }
            Chip {
                text: qsTr("About");       selected: root.activeTab === "about"
                onClicked: root.activeTab = "about"
            }

            Item { Layout.fillWidth: true }
        }

        Divider { Layout.fillWidth: true }

        // ── Active screen ─────────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            currentIndex: root.activeTab === "company"     ? 1
                        : root.activeTab === "backup"      ? 2
                        : root.activeTab === "support"     ? 3
                        : root.activeTab === "diagnostics" ? 4
                        : root.activeTab === "about"       ? 5 : 0

            GeneralSettings   {}   // index 0
            CompanySettings   {}   // index 1
            BackupScreen      {}   // index 2
            SupportCenter     {}   // index 3
            DiagnosticsScreen {}   // index 4
            AboutScreen { onEarlyAccessRequested: root.earlyAccessRequested() }   // index 5
        }
    }
}
