#include "ExportViewModel.h"
#include "ExportService.h"

#include "storage/StorageService.h"
#include "core/Invoice.h"

#include <QDir>
#include <QUrl>
#include <QDesktopServices>

ExportViewModel::ExportViewModel(QObject* parent) : QObject(parent) {}

QString ExportViewModel::exportsDir() const
{
    if (!StorageService::instance().isInitialized()) return QString();
    const QString dir = QString::fromStdString(StorageService::instance().dataDir()) + "/exports";
    QDir().mkpath(dir);
    return dir;
}

QString ExportViewModel::finish(const QString& path, bool ok)
{
    if (ok) {
        lastPath_   = path;
        statusText_ = tr("Saved to %1").arg(QDir::toNativeSeparators(path));
        emit statusChanged();
        emit exported(path);
        return path;
    }
    statusText_ = tr("Export failed — check that the folder is writable and the disk is not full.");
    emit statusChanged();
    emit exportFailed(statusText_);
    return QString();
}

// Sanitize an invoice number into a safe filename component.
static QString safeName(const QString& s)
{
    QString v;
    for (const QChar c : s) v += (c.isLetterOrNumber() || c == '-' || c == '_') ? c : QChar('-');
    return v.isEmpty() ? QStringLiteral("invoice") : v;
}

QString ExportViewModel::exportInvoice(int invoiceId)
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const Invoice inv = StorageService::instance().invoices().load(static_cast<uint32_t>(invoiceId));
    const QString path = dir + "/invoice-" + safeName(QString::fromUtf8(inv.getInvoiceNumber())) + ".csv";
    return finish(path, exportsvc::exportInvoiceCsv(static_cast<uint32_t>(invoiceId), path));
}

QString ExportViewModel::exportInvoicePdf(int invoiceId, const QString& lang)
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const Invoice inv = StorageService::instance().invoices().load(static_cast<uint32_t>(invoiceId));
    const QString l = (lang == QLatin1String("fr") || lang == QLatin1String("ar")) ? lang : QStringLiteral("en");
    const QString path = dir + "/invoice-" + safeName(QString::fromUtf8(inv.getInvoiceNumber())) + "-" + l + ".pdf";
    return finish(path, exportsvc::exportInvoicePdf(static_cast<uint32_t>(invoiceId), l, path));
}

QString ExportViewModel::exportCustomerStatement(int customerId)
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const QString path = dir + "/statement-customer-" + QString::number(customerId) + ".csv";
    return finish(path, exportsvc::exportCustomerStatementCsv(static_cast<uint32_t>(customerId), path));
}

QString ExportViewModel::exportOutstandingSummary()
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const QString path = dir + "/outstanding-balances.csv";
    return finish(path, exportsvc::exportOutstandingSummaryCsv(path));
}

QString ExportViewModel::exportTrialBalance()
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const QString path = dir + "/trial-balance.csv";
    return finish(path, exportsvc::exportTrialBalanceCsv(path));
}

QString ExportViewModel::exportIncomeStatement()
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const QString path = dir + "/income-statement.csv";
    return finish(path, exportsvc::exportIncomeStatementCsv(path));
}

QString ExportViewModel::exportTaxSummary()
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return finish(QString(), false);
    const QString path = dir + "/tax-summary.csv";
    return finish(path, exportsvc::exportTaxSummaryCsv(path));
}

bool ExportViewModel::openExportsFolder()
{
    const QString dir = exportsDir();
    if (dir.isEmpty()) return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}
