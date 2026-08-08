#pragma once
#include "Budget.h"
#include <QAbstractTableModel>
#include <vector>

class BudgetTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColCategory = 0, ColLimit, ColMonth, ColYear, ColCount };

    explicit BudgetTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Budget> rows);
    void appendRow(const Budget& b);
    void updateRow(int row, const Budget& b);
    const Budget& at(int row) const;

    int     rowCount   (const QModelIndex& = {}) const override;
    int     columnCount(const QModelIndex& = {}) const override;
    QVariant data      (const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

private:
    std::vector<Budget> rows_;
};
