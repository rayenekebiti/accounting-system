#ifndef QUICK_CUSTOMER_FILTER_PROXY_H
#define QUICK_CUSTOMER_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

class CustomerFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit CustomerFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString categoryFilter_;
    QString searchText_;
};

#endif // QUICK_CUSTOMER_FILTER_PROXY_H
