#ifndef UI_MODELS_CUSTOMER_TABLE_MODEL_H
#define UI_MODELS_CUSTOMER_TABLE_MODEL_H
#include "Customer.h"
#include <QAbstractTableModel>
#include <vector>

class CustomerTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColId = 0, ColName, ColEmail, ColPhone, ColTaxNumber, ColBalance, ColStatus, ColCount };

    explicit CustomerTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Customer> rows);
    void appendRow(const Customer& c);
    void updateRow(int row, const Customer& c);
    void removeRow(int row);                       // soft delete (sets isDeleted)
    const Customer& at(int row) const;
    int liveCount() const;                          // non-deleted rows

    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    std::vector<Customer> rows_;
};

#endif
