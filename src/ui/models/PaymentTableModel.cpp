#include "models/PaymentTableModel.h"
#include <QString>
#include <utility>

static const char* methodName(PaymentMethod m)
{
    switch (m) {
        case PAYMENT_CASH:  return "Cash";
        case PAYMENT_BANK:  return "Bank";
        case PAYMENT_CHECK: return "Check";
        case PAYMENT_CARD:  return "Card";
        default:            return "Unknown";
    }
}

static const char* partyName(PartyType t)
{
    switch (t) {
        case PARTY_CUSTOMER: return "Customer";
        case PARTY_SUPPLIER: return "Supplier";
        default:             return "Unknown";
    }
}

PaymentTableModel::PaymentTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void PaymentTableModel::setRows(std::vector<Payment> rows)
{
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

void PaymentTableModel::appendRow(const Payment& p)
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back(p);
    endInsertRows();
}

void PaymentTableModel::updateRow(int row, const Payment& p)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row] = p;
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

void PaymentTableModel::removeRow(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    rows_[row].setIsDeleted(true);
    emit dataChanged(index(row, 0), index(row, ColCount - 1));
}

const Payment& PaymentTableModel::at(int row) const           { return rows_.at(row); }

int PaymentTableModel::liveCount() const
{
    int n = 0;
    for (const auto& p : rows_)
        if (!p.getIsDeleted()) ++n;
    return n;
}

int PaymentTableModel::rowCount(const QModelIndex&) const     { return static_cast<int>(rows_.size()); }
int PaymentTableModel::columnCount(const QModelIndex&) const  { return ColCount; }

QVariant PaymentTableModel::data(const QModelIndex& idx, int role) const
{
    if (!idx.isValid() || idx.row() >= static_cast<int>(rows_.size())) return {};
    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

    const Payment& p = rows_[idx.row()];
    switch (idx.column()) {
        case ColNumber:    return QString::fromUtf8(p.getPaymentNumber());
        case ColDate:      return QString::fromUtf8(p.getDate());
        case ColParty:     return QString::asprintf("%s #%u", partyName(p.getPartyType()), p.getPartyId());
        case ColInvoiceId: return p.getInvoiceId() == 0
                                    ? QStringLiteral("—")
                                    : QString::number(p.getInvoiceId());
        case ColAmount:    return QString::asprintf("$%.2f", p.getAmount());
        case ColMethod:    return QString::fromUtf8(methodName(p.getMethod()));
    }
    return {};
}

QVariant PaymentTableModel::headerData(int section, Qt::Orientation orient, int role) const
{
    if (role != Qt::DisplayRole || orient != Qt::Horizontal) return {};
    switch (section) {
        case ColNumber:    return QStringLiteral("#");
        case ColDate:      return QStringLiteral("DATE");
        case ColParty:     return QStringLiteral("PARTY");
        case ColInvoiceId: return QStringLiteral("INVOICE");
        case ColAmount:    return QStringLiteral("AMOUNT");
        case ColMethod:    return QStringLiteral("METHOD");
    }
    return {};
}
