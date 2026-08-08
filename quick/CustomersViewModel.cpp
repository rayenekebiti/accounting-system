#include "CustomersViewModel.h"
#include "storage/StorageService.h"

CustomersViewModel::CustomersViewModel(CustomerListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new CustomerFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    // Keep filteredCount live: any change to the proxy's visible rows
    // (filter invalidation, source reset) re-notifies QML.
    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &CustomersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &CustomersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &CustomersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &CustomersViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* CustomersViewModel::listModel() const
{
    return proxy_;
}

int CustomersViewModel::totalCount() const
{
    return totalCustomers_;
}

int CustomersViewModel::filteredCount() const
{
    return proxy_->rowCount();
}

int CustomersViewModel::totalCustomers() const
{
    return totalCustomers_;
}

int CustomersViewModel::owingCount() const
{
    // owingCount is an alias for withBalanceCount (customers with balance > 0)
    return withBalanceCount_;
}

QString CustomersViewModel::outstandingText() const
{
    return QString("$%1").arg(outstandingSum_, 0, 'f', 2);
}

int CustomersViewModel::withBalanceCount() const
{
    return withBalanceCount_;
}

int CustomersViewModel::atRiskCount() const
{
    return atRiskCount_;
}

QString CustomersViewModel::totalPaidText() const { return QString("$%1").arg(paidSum_, 0, 'f', 2); }
QString CustomersViewModel::creditText()    const { return QString("$%1").arg(creditSum_, 0, 'f', 2); }

void CustomersViewModel::setCategoryFilter(const QString& category)
{
    proxy_->setCategoryFilter(category);
}

void CustomersViewModel::setSearchText(const QString& text)
{
    proxy_->setSearchText(text);
}

void CustomersViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
}

void CustomersViewModel::recomputeSummaries()
{
    totalCustomers_  = 0;
    withBalanceCount_ = 0;
    atRiskCount_     = 0;
    outstandingSum_  = 0.0;
    paidSum_         = 0.0;
    creditSum_       = 0.0;

    for (const CustomerListModel::Row& row : source_->rows()) {
        ++totalCustomers_;
        if (row.balance > 0.0) {
            ++withBalanceCount_;
            outstandingSum_ += row.balance;
        }
        if (row.atRisk)
            ++atRiskCount_;
    }

    // Paid + credit come from the SETTLEMENT ENGINE (authoritative), not the legacy aggregate.
    if (StorageService::instance().isInitialized()) {
        auto& aj = StorageService::instance().audit();
        for (const auto& p : aj.listPayments()) {
            paidSum_   += p.amountCents / 100.0;
            creditSum_ += aj.unallocatedFor(p.id) / 100.0;
        }
    }
}
