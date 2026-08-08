#ifndef QUICK_SUPPLIER_FILTER_PROXY_H
#define QUICK_SUPPLIER_FILTER_PROXY_H

#include <QSortFilterProxyModel>
#include <QString>

// Mirrors CustomerFilterProxy: category ("All" | "Owing") + case-insensitive search across
// name / email / phone / tax number.
class SupplierFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit SupplierFilterProxy(QObject* parent = nullptr);

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString categoryFilter_;
    QString searchText_;
};

#endif // QUICK_SUPPLIER_FILTER_PROXY_H
