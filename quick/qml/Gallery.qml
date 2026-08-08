import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

Rectangle {
    id: root
    color: Theme.color.canvas

    // Internal section-header helper (not a registered component)
    component SectionTitle: Text {
        Layout.fillWidth: true
        font.pixelSize: Theme.font.base
        font.weight: Theme.font.weightSemibold
        font.family: Theme.font.uiFamily
        color: Theme.color.textSecondary
        topPadding: Theme.space.lg
        bottomPadding: Theme.space.xs
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth

        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: Theme.space.sm

            // ── Padding top ───────────────────────────────────────────────────
            Item { Layout.preferredHeight: Theme.space.lg }

            // ── AppButton ────────────────────────────────────────────────────
            SectionTitle { text: "AppButton" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                AppButton { text: "Primary";   variant: "primary" }
                AppButton { text: "Secondary"; variant: "secondary" }
                AppButton { text: "Ghost";     variant: "ghost" }
                AppButton { text: "Danger";    variant: "danger" }
                AppButton { text: "Loading";   loading: true }
            }

            // ── IconButton ───────────────────────────────────────────────────
            SectionTitle { text: "IconButton" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                IconButton { content: "✎" }
                IconButton { content: "✕" }
                IconButton { content: "⋯" }
            }

            // ── Badge ────────────────────────────────────────────────────────
            SectionTitle { text: "Badge" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                Badge { text: "Neutral"; tone: "neutral" }
                Badge { text: "Brand";   tone: "brand" }
                Badge { text: "Income";  tone: "income" }
                Badge { text: "Expense"; tone: "expense" }
                Badge { text: "Pending"; tone: "pending" }
                Badge { text: "Info";    tone: "info" }
            }

            // ── Chip ─────────────────────────────────────────────────────────
            SectionTitle { text: "Chip" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                Chip { text: "Selected";   selected: true }
                Chip { text: "Unselected"; selected: false }
            }

            // ── AppTextField ─────────────────────────────────────────────────
            SectionTitle { text: "AppTextField" }
            AppTextField {
                Layout.leftMargin: Theme.space.lg
                Layout.preferredWidth: 280
                label: "Company name"
                placeholder: "Acme Corp"
            }

            // ── SearchField ──────────────────────────────────────────────────
            SectionTitle { text: "SearchField" }
            SearchField {
                Layout.leftMargin: Theme.space.lg
                Layout.preferredWidth: 280
                placeholder: "Search invoices…"
            }

            // ── Avatar ───────────────────────────────────────────────────────
            SectionTitle { text: "Avatar" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                Avatar { name: "Alice Martin" }
                Avatar { name: "Bob Tanaka" }
                Avatar { name: "Carol" }
                Avatar { name: "DX" }
            }

            // ── Divider ──────────────────────────────────────────────────────
            SectionTitle { text: "Divider" }
            Divider {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
            }

            // ── StatCard ─────────────────────────────────────────────────────
            SectionTitle { text: "StatCard" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.md
                StatCard { title: "Invoices";     value: 42 }
                StatCard { title: "Receivables";  value: "$12,400"; tone: "income" }
                StatCard { title: "Overdue";      value: 3;         tone: "expense"; caption: "Needs attention" }
            }

            // ── PageHeader ───────────────────────────────────────────────────
            SectionTitle { text: "PageHeader" }
            PageHeader {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                title: "Invoices"
                subtitle: "Manage your customer invoices"
                AppButton { text: "New Invoice"; variant: "primary" }
            }

            // ── FilterBar ────────────────────────────────────────────────────
            SectionTitle { text: "FilterBar" }
            FilterBar {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                Chip { text: "All";    selected: true }
                Chip { text: "Paid";   selected: false }
                Chip { text: "Unpaid"; selected: false }
            }

            // ── CurrencyAmount ───────────────────────────────────────────────
            SectionTitle { text: "CurrencyAmount" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.lg
                CurrencyAmount { amount: "1,200.00"; sign: "none" }
                CurrencyAmount { amount: "450.00";   sign: "pos" }
                CurrencyAmount { amount: "75.00";    sign: "neg" }
            }

            // ── StatusBadge ──────────────────────────────────────────────────
            SectionTitle { text: "StatusBadge" }
            RowLayout {
                Layout.leftMargin: Theme.space.lg
                spacing: Theme.space.sm
                StatusBadge { status: "Draft" }
                StatusBadge { status: "Posted" }
                StatusBadge { status: "Paid" }
                StatusBadge { status: "Overdue" }
                StatusBadge { status: "Void" }
            }

            // ── EmptyState ───────────────────────────────────────────────────
            SectionTitle { text: "EmptyState" }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                color: Theme.color.surface
                border.color: Theme.color.border
                border.width: 1
                radius: Theme.radius.card

                EmptyState {
                    anchors.fill: parent
                    title: "No invoices found"
                    description: "Create your first invoice to get started."
                    actionText: "New Invoice"
                    icon: "📄"
                    onActionClicked: console.log("Gallery: EmptyState action clicked")
                }
            }

            // ── ListRowCard ───────────────────────────────────────────────────
            SectionTitle { text: "ListRowCard" }
            ListRowCard {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space.lg
                Layout.rightMargin: Theme.space.lg
                onClicked: console.log("Gallery: ListRowCard clicked")

                RowLayout {
                    width: parent.width
                    spacing: Theme.space.md
                    Avatar { name: "Alice Martin" }
                    ColumnLayout {
                        spacing: Theme.space.xxs
                        Text {
                            text: "INV-0042"
                            color: Theme.color.textPrimary
                            font.pixelSize: Theme.font.md
                            font.weight: Theme.font.weightBold
                            font.family: Theme.font.uiFamily
                        }
                        Text {
                            text: "Alice Martin"
                            color: Theme.color.textSecondary
                            font.pixelSize: Theme.font.sm
                            font.family: Theme.font.uiFamily
                        }
                    }
                    Item { Layout.fillWidth: true }
                    CurrencyAmount { amount: "2,500.00"; sign: "none" }
                    StatusBadge { status: "Paid" }
                }
            }

            // ── Padding bottom ────────────────────────────────────────────────
            Item { Layout.preferredHeight: Theme.space.xxxl }
        }
    }
}
