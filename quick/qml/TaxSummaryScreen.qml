import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

// TaxSummaryScreen — the Tax tab of the Ledger workspace. The VAT/GST report (tax collected /
// recoverable / net payable, all derived from the ledger by the engine) plus the authoritative
// tax-code registry. Read-only reporting; codes are authored via the editor. Bound to `taxSummaryVm`.
Item {
    id: root

    signal newTaxCodeRequested()

    // Map the engine's tax-type KEY (English) to a translated label; VAT/GST stay as recognised
    // acronyms. i18n.language dependency re-runs the binding on a live language switch.
    function taxTypeLabel(t) {
        i18n.language
        return t === "VAT"        ? qsTr("VAT")
             : t === "GST"        ? qsTr("GST")
             : t === "Sales Tax"  ? qsTr("Sales Tax")
             : t === "Zero-rated" ? qsTr("Zero-rated")
             : t === "Exempt"     ? qsTr("Exempt")
             : t
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space.md

        PageHeader {
            Layout.fillWidth: true
            title:    qsTr("Tax Summary")
            subtitle: qsTr("VAT / GST — output tax, input tax, net payable")

            AppButton {
                text:    qsTr("Export CSV")
                variant: "secondary"
                onClicked: { exportVm.exportTaxSummary(); exportVm.openExportsFolder() }
            }
            AppButton {
                text:    qsTr("New tax code")
                variant: "primary"
                onClicked: root.newTaxCodeRequested()
            }
        }

        SummaryBar {
            objectName: "taxSummary"
            Layout.fillWidth: true

            MetricCell {
                objectName: "taxNetCell"
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                emphasis: true
                label: taxSummaryVm.netIsPayable ? qsTr("Net tax payable") : qsTr("Net tax refund")
                value: taxSummaryVm.netPayableText
                tone:  taxSummaryVm.netIsPayable ? "pending" : "income"
                sub:   qsTr("collected − recoverable")
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Tax collected")
                value: taxSummaryVm.collectedText
                sub:   qsTr("output tax on sales")
            }
            Divider { orientation: "vertical"; Layout.fillHeight: true }
            MetricCell {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                label: qsTr("Tax recoverable")
                value: taxSummaryVm.recoverableText
                sub:   qsTr("input tax on purchases")
            }
        }

        Text {
            text:           qsTr("Tax codes")
            font.pixelSize: Theme.font.sm
            font.weight:    Theme.font.weightSemibold
            font.family:    Theme.font.uiFamily
            color:          Theme.color.textSecondary
            Layout.fillWidth: true
        }

        Item {
            Layout.fillWidth:  true
            Layout.fillHeight: true

            EmptyState {
                anchors.fill: parent
                visible: taxSummaryVm.codeCount === 0
                icon:        "🧮"
                title:       qsTr("No tax codes")
                description: qsTr("Add a VAT / GST / Sales Tax code to apply on invoices and expenses.")
                actionText:  qsTr("New tax code")
                onActionClicked: root.newTaxCodeRequested()
            }

            ListView {
                objectName: "taxCodeList"
                anchors.fill: parent
                visible: taxSummaryVm.codeCount > 0
                model:   taxSummaryVm.codesModel
                spacing: Theme.space.sm
                clip:    true
                ScrollBar.vertical: ScrollBar {}

                delegate: ListRowCard {
                    width: ListView.view ? ListView.view.width : 0
                    padding: Theme.space.md

                    RowLayout {
                        width: parent.width
                        spacing: Theme.space.md

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space.xxs
                            Text {
                                text:            model.name
                                color:           Theme.color.textPrimary
                                font.pixelSize:  Theme.font.md
                                font.weight:     Theme.font.weightBold
                                font.family:     Theme.font.uiFamily
                                elide:           Text.ElideRight
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                            Text {
                                text:            qsTr("%1 · from %2").arg(root.taxTypeLabel(model.typeName)).arg(model.effectiveDate)
                                color:           Theme.color.textSecondary
                                font.pixelSize:  Theme.font.sm
                                font.family:     Theme.font.uiFamily
                                horizontalAlignment: Text.AlignLeft   // logical start — mirrors under RTL
                                Layout.fillWidth: true
                            }
                        }

                        Badge { tone: "info"; text: qsTr("v%1").arg(model.version) }

                        Text {
                            text:  model.rateText
                            color: Theme.color.textPrimary
                            font.pixelSize: Theme.font.md
                            font.weight:    Theme.font.weightBold
                            font.family:    Theme.font.numericFamily
                        }
                    }
                }
            }
        }
    }
}
