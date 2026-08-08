#include "LedgerEntriesModel.h"
#include "AccountsListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include <QHash>
#include <cstdlib>

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

LedgerEntriesModel::LedgerEntriesModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int LedgerEntriesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

void LedgerEntriesModel::setAccountFilter(uint32_t accountId)
{
    scopeAccountId_ = accountId;
    refresh();
}

QString LedgerEntriesModel::descriptionOf(const Row& r) const
{
    return r.reverses != 0xFFFFFFFFu
        ? tr("Reversal of #%1").arg(r.reverses)
        : tr("Entry #%1").arg(r.id);
}

QVariantList LedgerEntriesModel::postingsOf(const Row& r) const
{
    QVariantList out;
    for (const Posting& p : r.postings) {
        QVariantMap m;
        m.insert("accountId",  static_cast<int>(p.accountId));
        m.insert("account",    p.account);
        m.insert("typeName",   p.typeName);
        m.insert("debitText",  p.amountCents > 0 ? money(p.amountCents) : QString());
        m.insert("creditText", p.amountCents < 0 ? money(-p.amountCents) : QString());
        out.append(m);
    }
    return out;
}

QVariant LedgerEntriesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case EntryIdRole:          return static_cast<int>(r.id);
    case DateRole:             return r.date;
    case DescriptionRole:      return descriptionOf(r);
    case IsReversalRole:       return r.reverses   != 0xFFFFFFFFu;
    case IsReversedRole:       return r.reversedBy != 0xFFFFFFFFu;
    case LineCountRole:        return static_cast<int>(r.postings.size());
    case DebitTotalTextRole:   return money(r.debitTotal);
    case ScopedAmountTextRole:
        if (scopeAccountId_ == 0) return QString();
        return (r.scopedAmount >= 0 ? QStringLiteral("+") : QStringLiteral("−"))
               + money(std::llabs(r.scopedAmount));
    case PostingsRole:         return postingsOf(r);
    case SearchTextRole: {
        QString s = r.date + QLatin1Char(' ') + descriptionOf(r);
        for (const Posting& p : r.postings) s += QLatin1Char(' ') + p.account;
        return s;
    }
    default:                   return {};
    }
}

QHash<int, QByteArray> LedgerEntriesModel::roleNames() const
{
    return {
        { EntryIdRole,           "entryId"          },
        { DateRole,              "date"             },
        { DescriptionRole,       "description"      },
        { IsReversalRole,        "isReversal"       },
        { IsReversedRole,        "isReversed"       },
        { LineCountRole,         "lineCount"        },
        { DebitTotalTextRole,    "debitTotalText"   },
        { ScopedAmountTextRole,  "scopedAmountText" },
        { PostingsRole,          "postings"         },
    };
}

void LedgerEntriesModel::refresh()
{
    beginResetModel();
    rows_.clear();

    if (StorageService::instance().isInitialized()) {
        auto& aj = StorageService::instance().audit();

        // account id → (name, typeName) for display; the ledger keys on account id.
        QHash<uint32_t, QString> names;
        QHash<uint32_t, QString> types;
        for (const auto& a : aj.listAccounts()) {
            names.insert(a.id, QString::fromStdString(a.name));
            types.insert(a.id, AccountsListModel::typeName(a.type));
        }

        const auto entries = scopeAccountId_ == 0
            ? aj.listJournalEntries()
            : aj.entriesForAccount(scopeAccountId_);

        for (const auto& e : entries) {
            Row r;
            r.id         = e.id;
            r.date       = QString::fromStdString(e.effectiveDate.toString());
            r.reverses   = e.reverses;
            r.reversedBy = e.reversedBy;
            for (const auto& p : e.postings) {
                if (p.amountCents > 0) r.debitTotal += p.amountCents;
                if (scopeAccountId_ != 0 && p.accountId == scopeAccountId_)
                    r.scopedAmount += p.amountCents;
                r.postings.push_back({ p.accountId,
                                       names.value(p.accountId, QStringLiteral("—")),
                                       types.value(p.accountId, QStringLiteral("—")),
                                       p.amountCents });
            }
            rows_.push_back(std::move(r));
        }
    }

    endResetModel();
}
