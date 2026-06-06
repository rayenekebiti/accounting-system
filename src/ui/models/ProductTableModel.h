#ifndef UI_MODELS_PRODUCT_TABLE_MODEL_H
#define UI_MODELS_PRODUCT_TABLE_MODEL_H
#include "Product.h"
#include <QAbstractTableModel>
#include <vector>

class ProductTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColCode = 0, ColName, ColDescription, ColPrice, ColCost, ColStock, ColStatus, ColCount };

    explicit ProductTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Product> rows);
    void appendRow(const Product& p);
    void updateRow(int row, const Product& p);
    void removeRow(int row);
    const Product& at(int row) const;
    int liveCount() const;

    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    std::vector<Product> rows_;
};

#endif
