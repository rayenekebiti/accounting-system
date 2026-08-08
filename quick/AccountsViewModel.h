#ifndef QUICK_ACCOUNTS_VIEW_MODEL_H
#define QUICK_ACCOUNTS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "AccountsListModel.h"
#include "AccountsFilterProxy.h"

// Owns the accounts proxy + a summary derived ENTIRELY from the ledger engine
// (per-class rollups from listAccounts, trial-balance from trialBalanceTotal()).
class AccountsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel READ listModel CONSTANT)

    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(QString assetsText      READ assetsText      NOTIFY summaryChanged)
    Q_PROPERTY(QString liabilitiesText READ liabilitiesText NOTIFY summaryChanged)
    Q_PROPERTY(QString equityText      READ equityText      NOTIFY summaryChanged)
    Q_PROPERTY(QString incomeText      READ incomeText      NOTIFY summaryChanged)
    Q_PROPERTY(QString expenseText     READ expenseText     NOTIFY summaryChanged)

    Q_PROPERTY(QString trialBalanceText READ trialBalanceText NOTIFY summaryChanged)
    Q_PROPERTY(bool    isBalanced       READ isBalanced       NOTIFY summaryChanged)

public:
    explicit AccountsViewModel(AccountsListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int totalCount()    const;
    int filteredCount() const;

    QString assetsText()      const;
    QString liabilitiesText() const;
    QString equityText()      const;
    QString incomeText()      const;
    QString expenseText()     const;
    QString trialBalanceText() const;
    bool    isBalanced()       const;

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    AccountsListModel*   source_;
    AccountsFilterProxy* proxy_;

    int     totalCount_    = 0;
    int64_t assets_        = 0;
    int64_t liabilities_   = 0;
    int64_t equity_        = 0;
    int64_t income_        = 0;
    int64_t expense_       = 0;
    int64_t trialBalance_  = 0;
};

#endif // QUICK_ACCOUNTS_VIEW_MODEL_H
