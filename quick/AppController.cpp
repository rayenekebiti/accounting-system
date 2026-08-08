#include "AppController.h"
#include "core/Invoice.h"

AppController::AppController(const QString& dataPath, InvoiceListModel* model, QObject* parent)
    : QObject(parent)
    , model_(model)
    , dataPath_(dataPath)
{
    recomputeKpis();
}

int AppController::invoiceCount() const
{
    return invoiceCount_;
}

QString AppController::totalReceivablesText() const
{
    return QString("$%1").arg(totalReceivables_, 0, 'f', 2);
}

void AppController::setRtl(bool v)
{
    if (rtl_ == v) return;
    rtl_ = v;
    emit rtlChanged();
}

void AppController::refresh()
{
    model_->refresh();
    recomputeKpis();
    emit changed();
}

void AppController::toggleRtl()
{
    setRtl(!rtl_);
}

void AppController::recomputeKpis()
{
    invoiceCount_ = model_->rowCount();

    double receivables = 0.0;
    for (const Invoice& inv : model_->invoices()) {
        const InvoiceStatus s = inv.getStatus();
        if (s == INVOICE_POSTED || s == INVOICE_OVERDUE)
            receivables += inv.getTotal().toDouble();
    }
    totalReceivables_ = receivables;
}
