#include "models/BudgetTableModel.h"

BudgetTableModel::BudgetTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void BudgetTableModel::setRows(std::vector<Budget> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void BudgetTableModel::appendRow(const Budget& b)
{
    beginInsertRows({}, (int)rows_.size(), (int)rows_.size());
    rows_.push_back(b);
    endInsertRows();
}

void BudgetTableModel::updateRow(int row, const Budget& b)
{
    if (row < 0 || row >= (int)rows_.size()) return;
    rows_[row] = b;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Budget& BudgetTableModel::at(int row) const { return rows_[row]; }

int BudgetTableModel::rowCount   (const QModelIndex&) const { return (int)rows_.size(); }
int BudgetTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant BudgetTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= (int)rows_.size()) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
    const auto& b = rows_[idx.row()];
    switch (idx.column()) {
        case ColCategory: return QString("Cat #%1").arg(b.getCategoryId());
        case ColLimit:    return QString("$%1").arg(b.getMonthlyLimit(), 0, 'f', 2);
        case ColMonth:    return b.getMonth();
        case ColYear:     return b.getYear();
        default:          return {};
    }
}

QVariant BudgetTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColCategory: return "Category";
        case ColLimit:    return "Monthly Limit";
        case ColMonth:    return "Month";
        case ColYear:     return "Year";
        default:          return {};
    }
}
