#include "models/ProductTableModel.h"
#include <QString>
#include <utility>

ProductTableModel::ProductTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void ProductTableModel::setRows(std::vector<Product> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void ProductTableModel::appendRow(const Product& p)
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back(p);
    endInsertRows();
}

void ProductTableModel::updateRow(int row, const Product& p)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row] = p;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void ProductTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Product& ProductTableModel::at(int row) const           { return rows_.at(row); }

int ProductTableModel::liveCount() const
{
    int n = 0;
    for (const auto& p : rows_)
        if (!p.getIsDeleted()) ++n;
    return n;
}

int ProductTableModel::rowCount(const QModelIndex&) const     { return static_cast<int>(rows_.size()); }
int ProductTableModel::columnCount(const QModelIndex&) const  { return ColCount; }

QVariant ProductTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size())) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

    const Product& p = rows_[idx.row()];
    switch (idx.column()) {
        case ColCode:        return QString::fromUtf8(p.getCode());
        case ColName:        return QString::fromUtf8(p.getName());
        case ColDescription: return QString::fromUtf8(p.getDescription());
        case ColPrice:       return QString::asprintf("$%.2f", p.getPrice());
        case ColCost:        return QString::asprintf("$%.2f", p.getCost());
        case ColStock:       return p.getStock();
        case ColStatus: {
            if (p.getIsDeleted())   return QStringLiteral("Inactive");
            const int s = p.getStock();
            if (s <= 0)             return QStringLiteral("Out");
            if (s < 10)             return QStringLiteral("Low");
            return QStringLiteral("In Stock");
        }
    }
    return {};
}

QVariant ProductTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (role != Qt::DisplayRole || orient != Qt::Horizontal) return {};
    switch (section) {
        case ColCode:        return QStringLiteral("CODE");
        case ColName:        return QStringLiteral("NAME");
        case ColDescription: return QStringLiteral("DESCRIPTION");
        case ColPrice:       return QStringLiteral("PRICE");
        case ColCost:        return QStringLiteral("COST");
        case ColStock:       return QStringLiteral("STOCK");
        case ColStatus:      return QStringLiteral("STATUS");
    }
    return {};
}
