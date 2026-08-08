#include "AccountsListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include <cstdlib>

AccountsListModel::AccountsListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int AccountsListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QString AccountsListModel::typeName(uint8_t type)
{
    switch (type) {
    case AuditJournal::Asset:     return QStringLiteral("Asset");
    case AuditJournal::Liability: return QStringLiteral("Liability");
    case AuditJournal::Equity:    return QStringLiteral("Equity");
    case AuditJournal::Income:    return QStringLiteral("Income");
    case AuditJournal::Expense:   return QStringLiteral("Expense");
    default:                      return QStringLiteral("—");
    }
}

QString AccountsListModel::normalSide(uint8_t type)
{
    // Debit-normal accounts carry a positive Σ postings; credit-normal a negative one.
    return (type == AuditJournal::Asset || type == AuditJournal::Expense)
               ? QStringLiteral("Debit") : QStringLiteral("Credit");
}

QVariant AccountsListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case AccountIdRole:    return static_cast<int>(r.id);
    case NameRole:         return r.name;
    case TypeRole:         return static_cast<int>(r.type);
    case TypeNameRole:     return typeName(r.type);
    case BalanceTextRole:  return QString("$%1").arg(std::llabs(r.balanceCents) / 100.0, 0, 'f', 2);
    case BalanceCentsRole: return static_cast<qlonglong>(r.balanceCents);
    case NormalSideRole:   return normalSide(r.type);
    default:               return {};
    }
}

QHash<int, QByteArray> AccountsListModel::roleNames() const
{
    return {
        { AccountIdRole,    "accountId"    },
        { NameRole,         "name"         },
        { TypeRole,         "type"         },
        { TypeNameRole,     "typeName"     },
        { BalanceTextRole,  "balanceText"  },
        { BalanceCentsRole, "balanceCents" },
        { NormalSideRole,   "normalSide"   },
    };
}

void AccountsListModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        for (const auto& a : StorageService::instance().audit().listAccounts()) {
            Row r;
            r.id           = a.id;
            r.type         = a.type;
            r.name         = QString::fromStdString(a.name);
            r.balanceCents = a.balanceCents;
            rows_.push_back(std::move(r));
        }
    }

    endResetModel();
}
