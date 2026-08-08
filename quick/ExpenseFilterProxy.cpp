#include "ExpenseFilterProxy.h"
#include "ExpenseListModel.h"

ExpenseFilterProxy::ExpenseFilterProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{}

void ExpenseFilterProxy::setCategoryFilter(const QString& category)
{
    categoryFilter_ = category;
    invalidateFilter();
}

void ExpenseFilterProxy::setSearchText(const QString& text)
{
    searchText_ = text;
    invalidateFilter();
}

bool ExpenseFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* src = sourceModel();
    if (!src) return true;

    const QModelIndex idx = src->index(sourceRow, 0, sourceParent);

    if (!categoryFilter_.isEmpty() && categoryFilter_ != QStringLiteral("All")) {
        // A chip matches either a category or a payment method.
        const QString cat    = src->data(idx, ExpenseListModel::CategoryRole).toString();
        const QString method = src->data(idx, ExpenseListModel::PaymentMethodRole).toString();
        if (cat != categoryFilter_ && method != categoryFilter_)
            return false;
    }

    if (!searchText_.isEmpty()) {
        const QString sup    = src->data(idx, ExpenseListModel::SupplierRole).toString();
        const QString amount = src->data(idx, ExpenseListModel::AmountTextRole).toString();
        const QString date   = src->data(idx, ExpenseListModel::DateRole).toString();
        const QString memo   = src->data(idx, ExpenseListModel::MemoRole).toString();
        const QString cat    = src->data(idx, ExpenseListModel::CategoryRole).toString();
        if (!sup.contains(searchText_, Qt::CaseInsensitive) &&
            !amount.contains(searchText_, Qt::CaseInsensitive) &&
            !date.contains(searchText_, Qt::CaseInsensitive) &&
            !memo.contains(searchText_, Qt::CaseInsensitive) &&
            !cat.contains(searchText_, Qt::CaseInsensitive))
            return false;
    }

    return true;
}
