#include "TrialBalanceModel.h"
#include "AccountsListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include <cstdlib>

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

TrialBalanceModel::TrialBalanceModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TrialBalanceModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant TrialBalanceModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case AccountIdRole:  return static_cast<int>(r.id);
    case NameRole:       return r.name;
    case TypeNameRole:   return r.typeName;
    case DebitTextRole:  return r.debitCents  != 0 ? money(r.debitCents)  : QString();
    case CreditTextRole: return r.creditCents != 0 ? money(r.creditCents) : QString();
    default:             return {};
    }
}

QHash<int, QByteArray> TrialBalanceModel::roleNames() const
{
    return {
        { AccountIdRole, "accountId"  },
        { NameRole,      "name"       },
        { TypeNameRole,  "typeName"   },
        { DebitTextRole, "debitText"  },
        { CreditTextRole,"creditText" },
    };
}

QString TrialBalanceModel::totalDebitText()  const { return money(totalDebit_); }
QString TrialBalanceModel::totalCreditText() const { return money(totalCredit_); }
QString TrialBalanceModel::differenceText()  const { return money(std::llabs(trialTotal_)); }
bool    TrialBalanceModel::balanced()        const { return trialTotal_ == 0; }
int     TrialBalanceModel::accountCount()    const { return static_cast<int>(rows_.size()); }

void TrialBalanceModel::refresh()
{
    beginResetModel();
    rows_.clear();
    totalDebit_ = totalCredit_ = 0;

    if (StorageService::instance().isInitialized()) {
        auto& aj = StorageService::instance().audit();
        for (const auto& a : aj.listAccounts()) {
            const int64_t b = a.balanceCents;   // signed: debit +, credit −
            Row r;
            r.id          = a.id;
            r.name        = QString::fromStdString(a.name);
            r.typeName    = AccountsListModel::typeName(a.type);
            r.debitCents  = b > 0 ?  b : 0;
            r.creditCents = b < 0 ? -b : 0;
            totalDebit_  += r.debitCents;
            totalCredit_ += r.creditCents;
            rows_.push_back(std::move(r));
        }
        trialTotal_ = aj.trialBalanceTotal();
    }

    endResetModel();
    emit summaryChanged();
}
