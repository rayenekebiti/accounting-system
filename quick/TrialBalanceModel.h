#ifndef QUICK_TRIAL_BALANCE_MODEL_H
#define QUICK_TRIAL_BALANCE_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <vector>
#include <cstdint>

// One row per account with its debit/credit column (debit = balance>0, credit = −balance<0),
// straight from the ledger (AuditJournal::listAccounts / balanceFor). Carries its own summary
// (total debit == total credit, always, because the trial balance is 0) so no separate VM is
// needed. Read-only; nothing is stored or recomputed here beyond column placement.
class TrialBalanceModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(QString totalDebitText  READ totalDebitText  NOTIFY summaryChanged)
    Q_PROPERTY(QString totalCreditText READ totalCreditText NOTIFY summaryChanged)
    Q_PROPERTY(QString differenceText  READ differenceText  NOTIFY summaryChanged)
    Q_PROPERTY(bool    balanced        READ balanced        NOTIFY summaryChanged)
    Q_PROPERTY(int     accountCount    READ accountCount    NOTIFY summaryChanged)

public:
    struct Row {
        uint32_t id          = 0;
        QString  name;
        QString  typeName;
        int64_t  debitCents  = 0;
        int64_t  creditCents = 0;
    };

    enum Roles {
        AccountIdRole = Qt::UserRole + 1,
        NameRole,
        TypeNameRole,
        DebitTextRole,     // "" when this account is credit-balanced
        CreditTextRole     // "" when this account is debit-balanced
    };
    Q_ENUM(Roles)

    explicit TrialBalanceModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString totalDebitText()  const;
    QString totalCreditText() const;
    QString differenceText()  const;
    bool    balanced()        const;
    int     accountCount()    const;

    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();

private:
    std::vector<Row> rows_;
    int64_t totalDebit_  = 0;
    int64_t totalCredit_ = 0;
    int64_t trialTotal_  = 0;   // engine trialBalanceTotal() — invariant 0
};

#endif // QUICK_TRIAL_BALANCE_MODEL_H
