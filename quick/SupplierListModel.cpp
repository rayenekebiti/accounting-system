#include "SupplierListModel.h"
#include "storage/StorageService.h"
#include <QString>

SupplierListModel::SupplierListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int SupplierListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant SupplierListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    const Supplier& s = row.supplier;

    switch (role) {
    case SupplierIdRole:  return static_cast<int>(s.getId());
    case NameRole:        return QString::fromUtf8(s.getName());
    case EmailRole:       return QString::fromUtf8(s.getEmail());
    case PhoneRole:       return QString::fromUtf8(s.getPhone());
    case TaxNumberRole:   return QString::fromUtf8(s.getTaxNumber());
    case BalanceTextRole: return QString("$%1").arg(row.balance, 0, 'f', 2);
    case HasBalanceRole:  return row.balance > 0.0;
    default:              return {};
    }
}

QHash<int, QByteArray> SupplierListModel::roleNames() const
{
    return {
        { SupplierIdRole,  "supplierId"  },
        { NameRole,        "name"        },
        { EmailRole,       "email"       },
        { PhoneRole,       "phone"       },
        { TaxNumberRole,   "taxNumber"   },
        { BalanceTextRole, "balanceText" },
        { HasBalanceRole,  "hasBalance"  },
    };
}

void SupplierListModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        for (const Supplier& s : StorageService::instance().suppliers().loadAll()) {
            if (s.getIsDeleted()) continue;   // loadAll() already skips deleted; extra guard
            rows_.push_back({ s, s.getBalance().toDouble() });
        }
    }

    endResetModel();
}
