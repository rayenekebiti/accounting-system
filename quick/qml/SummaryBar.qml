import QtQuick
import QtQuick.Layouts
import App

// SummaryBar — Card shell + RowLayout for grouped KPI cells.
// Extracted from InvoicesScreen (rule-of-two: both Invoices and Customers use this).
// Usage:
//   SummaryBar {
//       MetricCell { ... }
//       Divider { orientation: "vertical"; Layout.fillHeight: true }
//       MetricCell { ... }
//   }
Card {
    id: root

    default property alias content: row.data
    property int spacing: Theme.space.lg

    padding: Theme.space.lg

    RowLayout {
        id: row
        width: parent.width
        spacing: root.spacing
    }
}
