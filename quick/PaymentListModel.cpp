#include "PaymentListModel.h"
#include "storage/StorageService.h"
#include <QHash>

PaymentListModel::PaymentListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int PaymentListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QString PaymentListModel::statusOf(int64_t amount, int64_t unallocated)
{
    if (unallocated >= amount) return QStringLiteral("Unallocated");
    if (unallocated <= 0)      return QStringLiteral("Allocated");
    return QStringLiteral("Partial");
}

QVariant PaymentListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case PaymentIdRole:       return static_cast<int>(r.id);
    case CustomerRole:        return r.customerName;
    case AmountTextRole:      return QString("$%1").arg(r.amountCents / 100.0, 0, 'f', 2);
    case DateRole:            return r.date;
    case StatusRole:          return statusOf(r.amountCents, r.unallocated);
    case UnallocatedTextRole: return QString("$%1").arg(r.unallocated / 100.0, 0, 'f', 2);
    case HasUnallocatedRole:  return r.unallocated > 0;
    default:                  return {};
    }
}

QHash<int, QByteArray> PaymentListModel::roleNames() const
{
    return {
        { PaymentIdRole,       "paymentId"       },
        { CustomerRole,        "customer"        },
        { AmountTextRole,      "amountText"      },
        { DateRole,            "date"            },
        { StatusRole,          "status"          },
        { UnallocatedTextRole, "unallocatedText" },
        { HasUnallocatedRole,  "hasUnallocated"  },
    };
}

void PaymentListModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        auto& storage = StorageService::instance();
        // Customer id → name (for display; the settlement engine keys on customer id).
        QHash<uint32_t, QString> names;
        for (const auto& c : storage.customers().loadAll())
            names.insert(c.getId(), QString::fromUtf8(c.getName()));

        for (const auto& p : storage.audit().listPayments()) {
            Row r;
            r.id          = p.id;
            r.customerId  = p.customerId;
            r.customerName = names.value(p.customerId, QStringLiteral("—"));
            r.amountCents = p.amountCents;
            r.unallocated = storage.audit().unallocatedFor(p.id);
            r.date        = QString::fromStdString(p.date.toString());
            rows_.push_back(std::move(r));
        }
    }

    endResetModel();
}
