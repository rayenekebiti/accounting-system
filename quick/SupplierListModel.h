#ifndef QUICK_SUPPLIER_LIST_MODEL_H
#define QUICK_SUPPLIER_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <vector>
#include "core/Supplier.h"

// Mirrors CustomerListModel. Suppliers are payables — the balance is the stored amount owed
// to the supplier (no invoice-derived aggregate / at-risk), so the model is a touch simpler.
class SupplierListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        Supplier supplier;
        double   balance = 0.0;
    };

    enum Roles {
        SupplierIdRole  = Qt::UserRole + 1,
        NameRole,
        EmailRole,
        PhoneRole,
        TaxNumberRole,
        BalanceTextRole,
        HasBalanceRole
    };
    Q_ENUM(Roles)

    explicit SupplierListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    const std::vector<Row>& rows() const { return rows_; }

private:
    std::vector<Row> rows_;
};

#endif // QUICK_SUPPLIER_LIST_MODEL_H
