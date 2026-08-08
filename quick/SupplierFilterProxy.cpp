#include "SupplierFilterProxy.h"
#include "SupplierListModel.h"

SupplierFilterProxy::SupplierFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void SupplierFilterProxy::setCategoryFilter(const QString& category)
{
    categoryFilter_ = category;
    invalidateFilter();
}

void SupplierFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool SupplierFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    // Category filter: ""|"All" = no filter, "Owing" = hasBalance true.
    if (!categoryFilter_.isEmpty() && categoryFilter_ != QStringLiteral("All")) {
        if (categoryFilter_ == QStringLiteral("Owing")) {
            if (!src->data(idx, SupplierListModel::HasBalanceRole).toBool())
                return false;
        }
    }

    // Search text: case-insensitive substring on name, email, phone, tax number.
    if (!searchText_.isEmpty()) {
        const QString name  = src->data(idx, SupplierListModel::NameRole).toString();
        const QString email = src->data(idx, SupplierListModel::EmailRole).toString();
        const QString phone = src->data(idx, SupplierListModel::PhoneRole).toString();
        const QString tax   = src->data(idx, SupplierListModel::TaxNumberRole).toString();
        if (!name.contains(searchText_, Qt::CaseInsensitive) &&
            !email.contains(searchText_, Qt::CaseInsensitive) &&
            !phone.contains(searchText_, Qt::CaseInsensitive) &&
            !tax.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
