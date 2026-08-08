#ifndef QUICK_ACCOUNTS_LIST_MODEL_H
#define QUICK_ACCOUNTS_LIST_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <vector>
#include <cstdint>

// Reads the authoritative ledger (AuditJournal::listAccounts); never a repository.
// Balance is DERIVED (Σ postings via balanceFor), never a stored running balance.
// Sign convention: DEBIT positive, CREDIT negative — so Asset/Expense carry a positive
// (debit-normal) balance and Liability/Equity/Income a negative (credit-normal) one.
class AccountsListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        uint32_t id           = 0;
        uint8_t  type         = 0;
        QString  name;
        int64_t  balanceCents = 0;
    };

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,            // int (1..5)
        TypeNameRole,        // "Asset" | "Liability" | "Equity" | "Income" | "Expense"
        BalanceTextRole,     // magnitude, e.g. "$1,234.00"
        BalanceCentsRole,    // signed cents (for sorting / sign)
        NormalSideRole       // "Debit" | "Credit"
    };
    Q_ENUM(Roles)

    explicit AccountsListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    const std::vector<Row>& rows() const { return rows_; }

    static QString typeName(uint8_t type);
    static QString normalSide(uint8_t type);   // debit-normal (Asset/Expense) vs credit-normal

private:
    std::vector<Row> rows_;
};

#endif // QUICK_ACCOUNTS_LIST_MODEL_H
