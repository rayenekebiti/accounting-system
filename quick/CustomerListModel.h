#ifndef QUICK_CUSTOMER_LIST_MODEL_H
#define QUICK_CUSTOMER_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <vector>
#include "core/Customer.h"

class CustomerListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        Customer customer;
        double   balance = 0.0;
        bool     atRisk  = false;
    };

    enum Roles {
        CustomerIdRole  = Qt::UserRole + 1,
        NameRole,
        EmailRole,
        PhoneRole,
        BalanceTextRole,
        HasBalanceRole,
        AtRiskRole
    };
    Q_ENUM(Roles)

    explicit CustomerListModel(QObject* parent = nullptr);

    int     rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    // Expose rows for summary computation in CustomersViewModel
    const std::vector<Row>& rows() const { return rows_; }

private:
    std::vector<Row> rows_;
};

#endif // QUICK_CUSTOMER_LIST_MODEL_H
