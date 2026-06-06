#include "models/InvoiceTableModel.h"
#include <QString>
#include <utility>

static const char* statusName(InvoiceStatus s)
{
    switch (s) {
        case INVOICE_DRAFT:   return "Draft";
        case INVOICE_POSTED:  return "Posted";
        case INVOICE_PAID:    return "Paid";
        case INVOICE_OVERDUE: return "Overdue";
        case INVOICE_VOID:    return "Void";
        default:              return "Unknown";
    }
}

InvoiceTableModel::InvoiceTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void InvoiceTableModel::setRows(std::vector<Invoice> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void InvoiceTableModel::appendRow(const Invoice& inv)
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back(inv);
    endInsertRows();
}

void InvoiceTableModel::updateRow(int row, const Invoice& inv)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row] = inv;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void InvoiceTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Invoice& InvoiceTableModel::at(int row) const           { return rows_.at(row); }

int InvoiceTableModel::liveCount() const
{
    int n = 0;
    for (const auto& i : rows_)
        if (!i.getIsDeleted()) ++n;
    return n;
}

int InvoiceTableModel::rowCount(const QModelIndex&) const     { return static_cast<int>(rows_.size()); }
int InvoiceTableModel::columnCount(const QModelIndex&) const  { return ColCount; }

QVariant InvoiceTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size())) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

    const Invoice& inv = rows_[idx.row()];
    switch (idx.column()) {
        case ColNumber:     return QString::fromUtf8(inv.getInvoiceNumber());
        case ColCustomerId: return inv.getCustomerId();
        case ColIssueDate:  return QString::fromUtf8(inv.getIssueDate());
        case ColDueDate:    return QString::fromUtf8(inv.getDueDate());
        case ColTotal:      return QString::asprintf("$%.2f", inv.getTotal());
        case ColStatus:     return QString::fromUtf8(statusName(inv.getStatus()));
    }
    return {};
}

QVariant InvoiceTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (role != Qt::DisplayRole || orient != Qt::Horizontal) return {};
    switch (section) {
        case ColNumber:     return QStringLiteral("#");
        case ColCustomerId: return QStringLiteral("CUSTOMER");
        case ColIssueDate:  return QStringLiteral("ISSUE DATE");
        case ColDueDate:    return QStringLiteral("DUE DATE");
        case ColTotal:      return QStringLiteral("TOTAL");
        case ColStatus:     return QStringLiteral("STATUS");
    }
    return {};
}
