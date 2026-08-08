#ifndef QUICK_INVOICE_FILTER_PROXY_H
#define QUICK_INVOICE_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

class InvoiceFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit InvoiceFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setStatusFilter(const QString& status);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString statusFilter_;
    QString searchText_;
};

#endif // QUICK_INVOICE_FILTER_PROXY_H
