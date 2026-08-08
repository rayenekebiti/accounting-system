#include "PaymentFilterProxy.h"
#include "PaymentListModel.h"

PaymentFilterProxy::PaymentFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void PaymentFilterProxy::setCategoryFilter(const QString& category)
{
    categoryFilter_ = category;
    invalidateFilter();
}

void PaymentFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool PaymentFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (!categoryFilter_.isEmpty() && categoryFilter_ != QStringLiteral("All")) {
        if (src->data(idx, PaymentListModel::StatusRole).toString() != categoryFilter_)
            return false;
    }

    if (!searchText_.isEmpty()) {
        const QString cust   = src->data(idx, PaymentListModel::CustomerRole).toString();
        const QString amount = src->data(idx, PaymentListModel::AmountTextRole).toString();
        const QString date   = src->data(idx, PaymentListModel::DateRole).toString();
        if (!cust.contains(searchText_, Qt::CaseInsensitive) &&
            !amount.contains(searchText_, Qt::CaseInsensitive) &&
            !date.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
