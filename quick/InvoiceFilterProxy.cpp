#include "InvoiceFilterProxy.h"
#include "InvoiceListModel.h"

InvoiceFilterProxy::InvoiceFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
}

void InvoiceFilterProxy::setStatusFilter(const QString& status)
{
    statusFilter_ = status;
    invalidateFilter();
}

void InvoiceFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool InvoiceFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src)
        return true;

    QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    // Status filter
    if (!statusFilter_.isEmpty() && statusFilter_ != QStringLiteral("All")) {
        QString status = src->data(idx, InvoiceListModel::StatusRole).toString();
        if (status != statusFilter_)
            return false;
    }

    // Search text filter
    if (!searchText_.isEmpty()) {
        QString number   = src->data(idx, InvoiceListModel::NumberRole).toString();
        QString customer = src->data(idx, InvoiceListModel::CustomerRole).toString();
        if (!number.contains(searchText_, Qt::CaseInsensitive) &&
            !customer.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
