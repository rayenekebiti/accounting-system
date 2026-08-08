#include "models/TransactionTableModel.h"

TransactionTableModel::TransactionTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void TransactionTableModel::setRows(std::vector<std::unique_ptr<Transaction>> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

const Transaction& TransactionTableModel::at(int row) const { return *rows_[row]; }
int TransactionTableModel::liveCount() const { return (int)rows_.size(); }
int TransactionTableModel::rowCount   (const QModelIndex&) const { return (int)rows_.size(); }
int TransactionTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant TransactionTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= (int)rows_.size()) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
    const auto& tx = *rows_[idx.row()];
    switch (idx.column()) {
        case ColId:       return tx.getId();
        case ColDesc:     return QString::fromUtf8(tx.getDescription());
        case ColAmount:   return QString("$%1").arg(tx.getAmount(), 0, 'f', 2);
        case ColDate:     return QString::fromUtf8(tx.getDate());
        case ColCategory: return QString("Cat #%1").arg(tx.getCategoryId());
        case ColType:     return typeName(tx.getType());
        default:          return {};
    }
}

QVariant TransactionTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColId:       return "ID";
        case ColDesc:     return "Description";
        case ColAmount:   return "Amount";
        case ColDate:     return "Date";
        case ColCategory: return "Category";
        case ColType:     return "Type";
        default:          return {};
    }
}

QString TransactionTableModel::typeName(TransactionType t)
{
    switch (t) {
        case INCOME:            return "Income";
        case EXPENSE:           return "Expense";
        case RECURRING_INCOME:  return "Recurring Income";
        case RECURRING_EXPENSE: return "Recurring Expense";
        default:                return "Unknown";
    }
}
