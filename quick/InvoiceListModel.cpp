#include "InvoiceListModel.h"
#include "storage/StorageService.h"
#include <QString>

InvoiceListModel::InvoiceListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int InvoiceListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(invoices_.size());
}

QVariant InvoiceListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(invoices_.size()))
        return {};

    const Invoice& inv = invoices_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case NumberRole:
        return QString::fromUtf8(inv.getInvoiceNumber());
    case CustomerRole: {
        auto it = customerNames_.find(inv.getCustomerId());
        if (it != customerNames_.end())
            return it.value();
        return QString("#%1").arg(inv.getCustomerId());
    }
    case IssueDateRole:
        return QString::fromStdString(inv.getIssueDate().toString());
    case DueDateRole:
        return QString::fromStdString(inv.getDueDate().toString());
    case TotalTextRole:
        return QString("$%1").arg(inv.getTotal().toDouble(), 0, 'f', 2);
    case StatusRole:
        return statusString(inv.getStatus());
    case InvoiceIdRole:
        return static_cast<int>(inv.getId());
    case OutstandingTextRole: {
        const int64_t o = outstandingCents_[static_cast<std::size_t>(index.row())];
        return QString("$%1").arg(o / 100.0, 0, 'f', 2);
    }
    case SettledTextRole: {
        const int64_t s = settledCents_[static_cast<std::size_t>(index.row())];
        return QString("$%1").arg(s / 100.0, 0, 'f', 2);
    }
    case PaymentStatusRole: {
        const int64_t settled = settledCents_[static_cast<std::size_t>(index.row())];
        const int64_t total   = inv.getTotal().cents();
        if (settled <= 0)       return QStringLiteral("Unpaid");
        if (settled >= total)   return QStringLiteral("Paid");
        return QStringLiteral("Partial");
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> InvoiceListModel::roleNames() const
{
    return {
        { NumberRole,    "number"    },
        { CustomerRole,  "customer"  },
        { IssueDateRole, "issueDate" },
        { DueDateRole,   "dueDate"   },
        { TotalTextRole, "totalText" },
        { StatusRole,    "status"    },
        { InvoiceIdRole, "invoiceId" },
        { OutstandingTextRole, "outstandingText" },
        { SettledTextRole,     "settledText"     },
        { PaymentStatusRole,   "paymentStatus"   },
    };
}

void InvoiceListModel::refresh()
{
    beginResetModel();

    customerNames_.clear();
    invoices_.clear();
    outstandingCents_.clear();
    settledCents_.clear();

    if (StorageService::instance().isInitialized()) {
        auto& storage = StorageService::instance();
        for (const Customer& c : storage.customers().loadAll()) {
            if (!c.getIsDeleted())
                customerNames_.insert(c.getId(), QString::fromUtf8(c.getName()));
        }
        for (const Invoice& inv : storage.invoices().loadAll()) {
            if (inv.getIsDeleted()) continue;
            invoices_.push_back(inv);
            // Settlement is DERIVED from the authoritative engine — never a stored balance.
            settledCents_.push_back(storage.audit().settledFor(inv.getId()));
            outstandingCents_.push_back(storage.audit().outstandingFor(inv.getId()));
        }
    }

    endResetModel();
}

QString InvoiceListModel::statusString(InvoiceStatus s)
{
    switch (s) {
    case INVOICE_DRAFT:   return QStringLiteral("Draft");
    case INVOICE_POSTED:  return QStringLiteral("Posted");
    case INVOICE_PAID:    return QStringLiteral("Paid");
    case INVOICE_OVERDUE: return QStringLiteral("Overdue");
    case INVOICE_VOID:    return QStringLiteral("Void");
    default:              return QStringLiteral("Unknown");
    }
}
