#ifndef QUICK_EXPENSE_FILTER_PROXY_H
#define QUICK_EXPENSE_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

// Search (supplier / amount / date / memo) + category filter ("All" | a category | "Credit" |
// "Cash"). One combined chip axis keeps the screen simple. Mirrors PaymentFilterProxy.
class ExpenseFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ExpenseFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString categoryFilter_;
    QString searchText_;
};

#endif // QUICK_EXPENSE_FILTER_PROXY_H
