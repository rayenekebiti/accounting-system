#ifndef UI_MODELS_SUPPLIER_TABLE_MODEL_H
#define UI_MODELS_SUPPLIER_TABLE_MODEL_H
#include "Supplier.h"
#include <QAbstractTableModel>
#include <vector>

class SupplierTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColId = 0, ColName, ColEmail, ColPhone, ColTaxNumber, ColBalance, ColStatus, ColCount };

    explicit SupplierTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Supplier> rows);
    void appendRow(const Supplier& s);
    void updateRow(int row, const Supplier& s);
    void removeRow(int row);
    const Supplier& at(int row) const;
    int liveCount() const;

    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    std::vector<Supplier> rows_;
};

#endif
