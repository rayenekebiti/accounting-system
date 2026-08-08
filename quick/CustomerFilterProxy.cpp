#include "CustomerFilterProxy.h"
#include "CustomerListModel.h"

CustomerFilterProxy::CustomerFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void CustomerFilterProxy::setCategoryFilter(const QString& category)
{
    categoryFilter_ = category;
    invalidateFilter();
}

void CustomerFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool CustomerFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src)
        return true;

    QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    // Category filter: ""|"All" = no filter, "Owing" = hasBalance true, "AtRisk" = atRisk true
    if (!categoryFilter_.isEmpty() && categoryFilter_ != QStringLiteral("All")) {
        if (categoryFilter_ == QStringLiteral("Owing")) {
            if (!src->data(idx, CustomerListModel::HasBalanceRole).toBool())
                return false;
        } else if (categoryFilter_ == QStringLiteral("AtRisk")) {
            if (!src->data(idx, CustomerListModel::AtRiskRole).toBool())
                return false;
        }
    }

    // Search text filter: case-insensitive substring match on name, email, phone
    if (!searchText_.isEmpty()) {
        const QString name  = src->data(idx, CustomerListModel::NameRole).toString();
        const QString email = src->data(idx, CustomerListModel::EmailRole).toString();
        const QString phone = src->data(idx, CustomerListModel::PhoneRole).toString();
        if (!name.contains(searchText_, Qt::CaseInsensitive) &&
            !email.contains(searchText_, Qt::CaseInsensitive) &&
            !phone.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
