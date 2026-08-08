#pragma once
#include "Category.h"
#include <QAbstractTableModel>
#include <vector>

class CategoryTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColName = 0, ColType, ColCount };

    explicit CategoryTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Category> rows);
    void appendRow(const Category& cat);
    void updateRow(int row, const Category& cat);
    const Category& at(int row) const;

    int     rowCount   (const QModelIndex& = {}) const override;
    int     columnCount(const QModelIndex& = {}) const override;
    QVariant data      (const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

private:
    std::vector<Category> rows_;
};
