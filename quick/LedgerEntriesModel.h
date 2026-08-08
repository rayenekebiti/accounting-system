#ifndef QUICK_LEDGER_ENTRIES_MODEL_H
#define QUICK_LEDGER_ENTRIES_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <QVariantList>
#include <vector>
#include <cstdint>

// Reads the authoritative ledger (AuditJournal::listJournalEntries / entriesForAccount);
// never a repository. Every posting/amount is engine-derived. When scoped to an account
// (setAccountFilter), only entries touching it are listed and each row exposes that
// account's signed posting amount.
class LedgerEntriesModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Posting {
        uint32_t accountId = 0;
        QString  account;      // account name
        QString  typeName;
        int64_t  amountCents = 0;   // signed: debit +, credit −
    };
    struct Row {
        uint32_t id         = 0;
        QString  date;
        uint32_t reverses   = 0xFFFFFFFFu;
        uint32_t reversedBy = 0xFFFFFFFFu;
        int64_t  debitTotal = 0;       // Σ positive postings (== |Σ negative|, entries balance)
        int64_t  scopedAmount = 0;     // this account's signed amount when scoped
        std::vector<Posting> postings;
    };

    enum Roles {
        EntryIdRole = Qt::UserRole + 1,
        DateRole,
        DescriptionRole,     // "Entry #N" | "Reversal of #M"
        IsReversalRole,      // this entry negates another
        IsReversedRole,      // another entry negates this one
        LineCountRole,
        DebitTotalTextRole,  // the entry's total (magnitude)
        ScopedAmountTextRole,// signed amount on the scoped account ("" when unscoped)
        PostingsRole,        // QVariantList of {accountId, account, typeName, debitText, creditText}
        SearchTextRole       // date + description + account names, concatenated (proxy filter key)
    };
    Q_ENUM(Roles)

    explicit LedgerEntriesModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // accountId 0 = all entries; else only entries touching that account.
    void setAccountFilter(uint32_t accountId);
    Q_INVOKABLE void refresh();

    const std::vector<Row>& rows() const { return rows_; }

private:
    QString descriptionOf(const Row& r) const;
    QVariantList postingsOf(const Row& r) const;

    uint32_t scopeAccountId_ = 0;
    std::vector<Row> rows_;
};

#endif // QUICK_LEDGER_ENTRIES_MODEL_H
