#include "AccountsFilterProxy.h"
#include "AccountsListModel.h"

AccountsFilterProxy::AccountsFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void AccountsFilterProxy::setCategoryFilter(const QString& category)
{
    categoryFilter_ = category;
    invalidateFilter();
}

void AccountsFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool AccountsFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (!categoryFilter_.isEmpty() && categoryFilter_ != QStringLiteral("All")) {
        if (src->data(idx, AccountsListModel::TypeNameRole).toString() != categoryFilter_)
            return false;
    }

    if (!searchText_.isEmpty()) {
        const QString name = src->data(idx, AccountsListModel::NameRole).toString();
        const QString type = src->data(idx, AccountsListModel::TypeNameRole).toString();
        if (!name.contains(searchText_, Qt::CaseInsensitive) &&
            !type.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
