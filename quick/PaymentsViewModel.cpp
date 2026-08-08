#include "PaymentsViewModel.h"

PaymentsViewModel::PaymentsViewModel(PaymentListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new PaymentFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &PaymentsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &PaymentsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &PaymentsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &PaymentsViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* PaymentsViewModel::listModel() const { return proxy_; }

int     PaymentsViewModel::totalCount()        const { return totalPayments_; }
int     PaymentsViewModel::filteredCount()     const { return proxy_->rowCount(); }
int     PaymentsViewModel::totalPayments()     const { return totalPayments_; }
QString PaymentsViewModel::totalReceivedText() const { return QString("$%1").arg(receivedSum_ / 100.0, 0, 'f', 2); }
QString PaymentsViewModel::unallocatedText()   const { return QString("$%1").arg(unallocatedSum_ / 100.0, 0, 'f', 2); }
int     PaymentsViewModel::unallocatedCount()  const { return unallocatedCount_; }

void PaymentsViewModel::setCategoryFilter(const QString& category) { proxy_->setCategoryFilter(category); }
void PaymentsViewModel::setSearchText(const QString& text)        { proxy_->setSearchText(text); }

void PaymentsViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
}

void PaymentsViewModel::recomputeSummaries()
{
    totalPayments_    = 0;
    unallocatedCount_ = 0;
    receivedSum_      = 0;
    unallocatedSum_   = 0;

    for (const PaymentListModel::Row& r : source_->rows()) {
        ++totalPayments_;
        receivedSum_ += r.amountCents;
        if (r.unallocated > 0) {
            ++unallocatedCount_;
            unallocatedSum_ += r.unallocated;
        }
    }
}
