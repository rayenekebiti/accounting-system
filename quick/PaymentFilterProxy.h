#ifndef QUICK_PAYMENT_FILTER_PROXY_H
#define QUICK_PAYMENT_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

// Search (customer / amount / date) + status category ("All" | "Unallocated" | "Partial" | "Allocated").
class PaymentFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit PaymentFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString categoryFilter_;
    QString searchText_;
};

#endif // QUICK_PAYMENT_FILTER_PROXY_H
