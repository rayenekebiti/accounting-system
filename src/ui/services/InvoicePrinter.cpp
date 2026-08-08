#include "services/InvoicePrinter.h"
#include "Invoice.h"
#include "Customer.h"
#include "InvoiceLine.h"
#include "storage/StorageService.h"
#include <QTextDocument>
#include <QPrinter>
#include <QPrintDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>

static QString esc(const char* s) { return QString::fromUtf8(s).toHtmlEscaped(); }
static QString fmtM(Money m)      { return QString("$%1").arg(m.toDouble(), 0, 'f', 2); }

static QString statusStr(InvoiceStatus s)
{
    switch (s) {
        case INVOICE_DRAFT:   return "Draft";
        case INVOICE_POSTED:  return "Posted";
        case INVOICE_PAID:    return "Paid";
        case INVOICE_OVERDUE: return "Overdue";
        case INVOICE_VOID:    return "Void";
        default:              return "";
    }
}

QString InvoicePrinter::buildHtml(const Invoice& inv, const Customer& cust)
{
    std::vector<InvoiceLine> lines;
    if (StorageService::instance().isInitialized())
        lines = StorageService::instance().invoiceLines().findByInvoice(inv.getId());

    QString linesHtml;
    for (const auto& ln : lines) {
        linesHtml += QString(
            "<tr>"
            "<td style='padding:6px 8px;'>%1</td>"
            "<td style='padding:6px 8px;text-align:right;'>%2</td>"
            "<td style='padding:6px 8px;text-align:right;'>%3</td>"
            "<td style='padding:6px 8px;text-align:right;'>%4</td>"
            "<td style='padding:6px 8px;text-align:right;'>%5</td>"
            "</tr>")
            .arg(esc(ln.getDescription()))
            .arg(QString::number(ln.getQuantity(), 'f', 2))
            .arg(fmtM(ln.getUnitPrice()))
            .arg(QString("%1%").arg(ln.getTaxRatePermille() / 10.0, 0, 'f', 1))
            .arg(fmtM(ln.getLineTotal()));
    }

    return QString(R"html(
<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style>
  body { font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; color: #1a1a1a; }
  h1   { font-size: 28px; color: #2563EB; margin: 0 0 4px; }
  .meta { font-size: 11px; color: #6b7280; }
  table { width: 100%; border-collapse: collapse; margin-top: 16px; }
  th { background: #f3f4f6; text-align: left; padding: 6px 8px; border-bottom: 2px solid #e5e7eb; }
  td { border-bottom: 1px solid #e5e7eb; }
  .total-row td { font-weight: 600; background: #f9fafb; }
  .status { display:inline-block; padding: 2px 8px; border-radius: 4px;
            background: #dbeafe; color: #1e40af; font-weight: 600; }
  .bill { margin-top: 24px; }
  .totals { margin-top: 8px; float: right; }
  .totals td { padding: 4px 8px; }
  .totals .lbl { color: #6b7280; }
  .clearfix::after { content:''; display:block; clear:both; }
</style>
</head><body>
<h1>INVOICE</h1>
<p class="meta">
  <b>%1</b> &nbsp;|&nbsp;
  Issued: %2 &nbsp;|&nbsp;
  Due: %3 &nbsp;|&nbsp;
  <span class="status">%4</span>
</p>
<div class="bill">
  <b>Bill To:</b><br>
  %5<br>%6<br>%7
</div>
<table>
  <thead>
    <tr>
      <th>Description</th>
      <th style="text-align:right">Qty</th>
      <th style="text-align:right">Unit Price</th>
      <th style="text-align:right">Tax</th>
      <th style="text-align:right">Total</th>
    </tr>
  </thead>
  <tbody>%8</tbody>
</table>
<div class="clearfix">
  <table class="totals">
    <tr><td class="lbl">Subtotal</td><td style="text-align:right">%9</td></tr>
    <tr><td class="lbl">Tax</td>     <td style="text-align:right">%10</td></tr>
    <tr class="total-row"><td>Total</td><td style="text-align:right">%11</td></tr>
  </table>
</div>
</body></html>
)html")
    .arg(esc(inv.getInvoiceNumber()))
    .arg(QString::fromStdString(inv.getIssueDate().toString()))
    .arg(QString::fromStdString(inv.getDueDate().toString()))
    .arg(statusStr(inv.getStatus()))
    .arg(esc(cust.getName()))
    .arg(esc(cust.getEmail()))
    .arg(esc(cust.getPhone()))
    .arg(linesHtml)
    .arg(fmtM(inv.getSubtotal()))
    .arg(fmtM(inv.getTaxAmount()))
    .arg(fmtM(inv.getTotal()));
}

void InvoicePrinter::print(const Invoice& inv, const Customer& cust, QWidget* parent)
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dlg(&printer, parent);
    dlg.setWindowTitle("Print Invoice");
    if (dlg.exec() != QDialog::Accepted) return;

    QTextDocument doc;
    doc.setHtml(buildHtml(inv, cust));
    doc.print(&printer);
}

void InvoicePrinter::exportPdf(const Invoice& inv, const Customer& cust, QWidget* parent)
{
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export Invoice PDF", QString(), "PDF Files (*.pdf)");
    if (path.isEmpty()) return;

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);

    QTextDocument doc;
    doc.setHtml(buildHtml(inv, cust));
    doc.print(&printer);
}
