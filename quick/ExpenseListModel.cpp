#include "ExpenseListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/Expense.h"
#include <QHash>
#include <cstdlib>

ExpenseListModel::ExpenseListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int ExpenseListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QString ExpenseListModel::categoryName(uint8_t c)
{
    switch (c) {
    case EXPENSE_CAT_OFFICE:    return QStringLiteral("Office");
    case EXPENSE_CAT_RENT:      return QStringLiteral("Rent");
    case EXPENSE_CAT_UTILITIES: return QStringLiteral("Utilities");
    case EXPENSE_CAT_TRAVEL:    return QStringLiteral("Travel");
    default:                    return QStringLiteral("Other");
    }
}

QString ExpenseListModel::methodName(uint8_t m)
{
    return m == EXPENSE_PAY_CREDIT ? QStringLiteral("Credit") : QStringLiteral("Cash");
}

QVariant ExpenseListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case ExpenseIdRole:      return static_cast<int>(r.id);
    case SupplierRole:       return r.supplierName;
    case AmountTextRole:     return QString("$%1").arg(std::llabs(r.amountCents) / 100.0, 0, 'f', 2);
    case DateRole:           return r.date;
    case CategoryRole:       return categoryName(r.category);
    case PaymentMethodRole:  return methodName(r.paymentMethod);
    case StatusRole:         return r.status == 1 ? QStringLiteral("Void") : QStringLiteral("Active");
    case IsVoidRole:         return r.status == 1;
    case IsReversalRole:     return r.amountCents < 0;
    case IsReversedRole:     return r.reversed;
    case MemoRole:           return r.memo;
    default:                 return {};
    }
}

QHash<int, QByteArray> ExpenseListModel::roleNames() const
{
    return {
        { ExpenseIdRole,     "expenseId"     },
        { SupplierRole,      "supplier"      },
        { AmountTextRole,    "amountText"    },
        { DateRole,          "date"          },
        { CategoryRole,      "category"      },
        { PaymentMethodRole, "paymentMethod" },
        { StatusRole,        "status"        },
        { IsVoidRole,        "isVoid"        },
        { IsReversalRole,    "isReversal"    },
        { IsReversedRole,    "isReversed"    },
        { MemoRole,          "memo"          },
    };
}

void ExpenseListModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        auto& storage = StorageService::instance();

        // Supplier id → name (display join; the expense keys on supplier id).
        QHash<uint32_t, QString> names;
        for (const auto& s : storage.suppliers().loadAll())
            names.insert(s.getId(), QString::fromUtf8(s.getName()));

        for (const auto& e : storage.expenses().loadAll()) {
            Row r;
            r.id            = e.getId();
            r.supplierId    = e.getSupplierId();
            r.supplierName  = e.getSupplierId() == EXPENSE_NO_SUPPLIER
                                  ? QStringLiteral("—")
                                  : names.value(e.getSupplierId(), QStringLiteral("—"));
            r.amountCents   = e.getAmount().cents();
            r.date          = QString::fromStdString(e.getDate().toString());
            r.category      = e.getCategory();
            r.paymentMethod = e.getPaymentMethod();
            r.status        = e.getStatus();
            r.memo          = QString::fromUtf8(e.getMemo());
            r.reversed      = storage.audit().expenseReversedBy(e.getId()) != 0xFFFFFFFFu;
            rows_.push_back(std::move(r));
        }
    }

    endResetModel();
}
