#include "CustomerListModel.h"
#include "storage/StorageService.h"
#include <QString>

CustomerListModel::CustomerListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int CustomerListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant CustomerListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};

    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    const Customer& c = row.customer;

    switch (role) {
    case CustomerIdRole:
        return static_cast<int>(c.getId());
    case NameRole:
        return QString::fromUtf8(c.getName());
    case EmailRole:
        return QString::fromUtf8(c.getEmail());
    case PhoneRole:
        return QString::fromUtf8(c.getPhone());
    case BalanceTextRole:
        return QString("$%1").arg(row.balance, 0, 'f', 2);
    case HasBalanceRole:
        return row.balance > 0.0;
    case AtRiskRole:
        return row.atRisk;
    default:
        return {};
    }
}

QHash<int, QByteArray> CustomerListModel::roleNames() const
{
    return {
        { CustomerIdRole,  "customerId"   },
        { NameRole,        "name"         },
        { EmailRole,       "email"        },
        { PhoneRole,       "phone"        },
        { BalanceTextRole, "balanceText"  },
        { HasBalanceRole,  "hasBalance"   },
        { AtRiskRole,      "atRisk"       },
    };
}

void CustomerListModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        auto agg = StorageService::instance().computeCustomerAggregates();
        for (const Customer& c : StorageService::instance().customers().loadAll()) {
            // loadAll() already skips deleted; extra guard for safety
            if (c.getIsDeleted()) continue;
            const uint32_t id = c.getId();
            auto it = agg.find(id);
            double balance = 0.0;
            bool   atRisk  = false;
            if (it != agg.end()) {
                balance = it->second.balance.toDouble();
                atRisk  = it->second.hasOverdue;
            }
            rows_.push_back({ c, balance, atRisk });
        }
    }

    endResetModel();
}
