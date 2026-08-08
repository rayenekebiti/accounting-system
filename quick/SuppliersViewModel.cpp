#include "SuppliersViewModel.h"

SuppliersViewModel::SuppliersViewModel(SupplierListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new SupplierFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &SuppliersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &SuppliersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &SuppliersViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &SuppliersViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* SuppliersViewModel::listModel() const { return proxy_; }

int     SuppliersViewModel::totalCount()       const { return totalSuppliers_; }
int     SuppliersViewModel::filteredCount()    const { return proxy_->rowCount(); }
int     SuppliersViewModel::totalSuppliers()   const { return totalSuppliers_; }
int     SuppliersViewModel::owingCount()       const { return withBalanceCount_; }
QString SuppliersViewModel::outstandingText()  const { return QString("$%1").arg(outstandingSum_, 0, 'f', 2); }
int     SuppliersViewModel::withBalanceCount() const { return withBalanceCount_; }

void SuppliersViewModel::setCategoryFilter(const QString& category) { proxy_->setCategoryFilter(category); }
void SuppliersViewModel::setSearchText(const QString& text)        { proxy_->setSearchText(text); }

void SuppliersViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
}

void SuppliersViewModel::recomputeSummaries()
{
    totalSuppliers_   = 0;
    withBalanceCount_ = 0;
    outstandingSum_   = 0.0;

    for (const SupplierListModel::Row& row : source_->rows()) {
        ++totalSuppliers_;
        if (row.balance > 0.0) {
            ++withBalanceCount_;
            outstandingSum_ += row.balance;
        }
    }
}
