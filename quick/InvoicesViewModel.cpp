#include "InvoicesViewModel.h"
#include "core/Invoice.h"

InvoicesViewModel::InvoicesViewModel(InvoiceListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new InvoiceFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    // Keep filteredCount live: any change to the proxy's visible rows
    // (filter invalidation, source reset) re-notifies QML.
    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &InvoicesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &InvoicesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &InvoicesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &InvoicesViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* InvoicesViewModel::listModel() const
{
    return proxy_;
}

int InvoicesViewModel::totalCount() const
{
    return totalCount_;
}

int InvoicesViewModel::filteredCount() const
{
    return proxy_->rowCount();
}

QString InvoicesViewModel::outstandingText() const
{
    return QString("$%1").arg(outstandingSum_, 0, 'f', 2);
}

int InvoicesViewModel::outstandingCount() const
{
    return outstandingCount_;
}

QString InvoicesViewModel::overdueText() const
{
    return QString("$%1").arg(overdueSum_, 0, 'f', 2);
}

int InvoicesViewModel::overdueCount() const
{
    return overdueCount_;
}

QString InvoicesViewModel::paidText() const
{
    return QString("$%1").arg(paidSum_, 0, 'f', 2);
}

int InvoicesViewModel::paidCount() const
{
    return paidCount_;
}

int InvoicesViewModel::awaitingCount() const
{
    return awaitingCount_;
}

int InvoicesViewModel::draftCount() const
{
    return draftCount_;
}

void InvoicesViewModel::setStatusFilter(const QString& status)
{
    proxy_->setStatusFilter(status);
    emit filterChanged();
}

void InvoicesViewModel::setSearchText(const QString& text)
{
    proxy_->setSearchText(text);
    emit filterChanged();
}

void InvoicesViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
}

void InvoicesViewModel::recomputeSummaries()
{
    outstandingSum_   = 0.0;
    outstandingCount_ = 0;
    overdueSum_       = 0.0;
    overdueCount_     = 0;
    paidSum_          = 0.0;
    paidCount_        = 0;
    totalCount_       = 0;
    awaitingCount_    = 0;
    draftCount_       = 0;

    for (const Invoice& inv : source_->invoices()) {
        ++totalCount_;
        const InvoiceStatus s = inv.getStatus();
        const double amt = inv.getTotal().toDouble();

        switch (s) {
        case INVOICE_POSTED:
            outstandingSum_   += amt;
            ++outstandingCount_;
            ++awaitingCount_;
            break;
        case INVOICE_OVERDUE:
            outstandingSum_   += amt;
            ++outstandingCount_;
            overdueSum_       += amt;
            ++overdueCount_;
            ++awaitingCount_;
            break;
        case INVOICE_PAID:
            paidSum_ += amt;
            ++paidCount_;
            break;
        case INVOICE_DRAFT:
            ++draftCount_;
            break;
        default:
            break;
        }
    }
}
