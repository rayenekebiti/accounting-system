#ifndef QUICK_ACCOUNTS_FILTER_PROXY_H
#define QUICK_ACCOUNTS_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

// Search (account name) + type category ("All" | "Asset" | "Liability" | "Equity" |
// "Income" | "Expense"). Mirrors PaymentFilterProxy.
class AccountsFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AccountsFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString categoryFilter_;
    QString searchText_;
};

#endif // QUICK_ACCOUNTS_FILTER_PROXY_H
