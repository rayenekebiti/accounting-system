#include "models/CustomerTableModel.h"
#include <QString>
#include <utility>

CustomerTableModel::CustomerTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void CustomerTableModel::setRows(std::vector<Customer> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void CustomerTableModel::appendRow(const Customer& c)
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back(c);
    endInsertRows();
}

void CustomerTableModel::updateRow(int row, const Customer& c)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row] = c;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void CustomerTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Customer& CustomerTableModel::at(int row) const
{
    return rows_.at(row);
}

int CustomerTableModel::liveCount() const
{
    int n = 0;
    for (const auto& c : rows_)
        if (!c.getIsDeleted()) ++n;
    return n;
}

int CustomerTableModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(rows_.size());
}

int CustomerTableModel::columnCount(const QModelIndex&) const
{
    return ColCount;
}

QVariant CustomerTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size()))
        return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole)
        return {};

    const Customer& c = rows_[idx.row()];
    switch (idx.column()) {
        case ColId:        return c.getId();
        case ColName:      return QString::fromUtf8(c.getName());
        case ColEmail:     return QString::fromUtf8(c.getEmail());
        case ColPhone:     return QString::fromUtf8(c.getPhone());
        case ColTaxNumber: return QString::fromUtf8(c.getTaxNumber());
        case ColBalance:   return QString::asprintf("$%.2f", c.getBalance());
        case ColStatus:    return c.getIsDeleted() ? QStringLiteral("Inactive")
                                                   : QStringLiteral("Active");
    }
    return {};
}

QVariant CustomerTableModel::headerData(int section, Qt::Orientation orient, int role) const
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
