#include "models/AccountTableModel.h"

AccountTableModel::AccountTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

void AccountTableModel::setRows(std::vector<std::unique_ptr<Account>> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

const Account& AccountTableModel::at(int row) const { return *rows_[row]; }
int AccountTableModel::liveCount() const { return (int)rows_.size(); }
int AccountTableModel::rowCount   (const QModelIndex&) const { return (int)rows_.size(); }
int AccountTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant AccountTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= (int)rows_.size()) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};
    const auto& acc = *rows_[idx.row()];
    switch (idx.column()) {
        case ColName:    return QString::fromUtf8(acc.getName());
        case ColType:    return typeName(acc.getAccountType());
        case ColBalance: return QString("$%1").arg(acc.getBalance(), 0, 'f', 2);
        default:         return {};
    }
}

QVariant AccountTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (orient != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
        case ColName:    return "Name";
        case ColType:    return "Type";
        case ColBalance: return "Balance";
        default:         return {};
    }
}

QString AccountTableModel::typeName(AccountType t)
{
    switch (t) {
        case CASH:     return "Cash";
        case SAVINGS:  return "Savings";
        case CHECKING: return "Checking";
        case BANK:     return "Bank";
        default:       return "Unknown";
    }
}
