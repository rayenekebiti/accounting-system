#include "models/SupplierTableModel.h"
#include <QString>
#include <utility>

SupplierTableModel::SupplierTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void SupplierTableModel::setRows(std::vector<Supplier> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void SupplierTableModel::appendRow(const Supplier& s)
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back(s);
    endInsertRows();
}

void SupplierTableModel::updateRow(int row, const Supplier& s)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row] = s;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void SupplierTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Supplier& SupplierTableModel::at(int row) const
{
    return rows_.at(row);
}

int SupplierTableModel::liveCount() const
{
    int n = 0;
    for (const auto& s : rows_)
        if (!s.getIsDeleted()) ++n;
    return n;
}

int SupplierTableModel::rowCount(const QModelIndex&) const  { return static_cast<int>(rows_.size()); }
int SupplierTableModel::columnCount(const QModelIndex&) const { return ColCount; }

QVariant SupplierTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size())) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

    const Supplier& s = rows_[idx.row()];
    switch (idx.column()) {
        case ColId:        return s.getId();
        case ColName:      return QString::fromUtf8(s.getName());
        case ColEmail:     return QString::fromUtf8(s.getEmail());
        case ColPhone:     return QString::fromUtf8(s.getPhone());
        case ColTaxNumber: return QString::fromUtf8(s.getTaxNumber());
        case ColBalance:   return QString::asprintf("$%.2f", s.getBalance());
        case ColStatus:    return s.getIsDeleted() ? QStringLiteral("Inactive")
                                                   : QStringLiteral("Active");
    }
    return {};
}

QVariant SupplierTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (role != Qt::DisplayRole || orient != Qt::Horizontal) return {};
    switch (section) {
        case ColId:        return QStringLiteral("#");
        case ColName:      return QStringLiteral("NAME");
        case ColEmail:     return QStringLiteral("EMAIL");
        case ColPhone:     return QStringLiteral("PHONE");
        case ColTaxNumber: return QStringLiteral("TAX #");
        case ColBalance:   return QStringLiteral("BALANCE");
        case ColStatus:    return QStringLiteral("STATUS");
    }
    return {};
}
