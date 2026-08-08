#include "models/CategoryTableModel.h"

CategoryTableModel::CategoryTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void CategoryTableModel::setRows(std::vector<Category> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void CategoryTableModel::appendRow(const Category& cat)
{
    beginInsertRows({}, (int)rows_.size(), (int)rows_.size());
    rows_.push_back(cat);
    endInsertRows();
}

void CategoryTableModel::updateRow(int row, const Category& cat)
{
    if (row < 0 || row >= (int)rows_.size()) return;
    rows_[row] = cat;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Category& CategoryTableModel::at(int row) const { return rows_[row]; }

int CategoryTableModel::rowCount   (const QModelIndex&) const { return (int)rows_.size(); }
int CategoryTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant CategoryTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= (int)rows_.size()) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
    const auto& c = rows_[idx.row()];
    switch (idx.column()) {
        case ColName: return QString::fromUtf8(c.getName());
        case ColType: return c.getType() == INCOME ? "Income" : "Expense";
        default:      return {};
    }
}

QVariant CategoryTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColName: return "Name";
        case ColType: return "Type";
        default:      return {};
    }
}
