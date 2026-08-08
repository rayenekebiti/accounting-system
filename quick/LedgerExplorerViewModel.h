#ifndef QUICK_LEDGER_EXPLORER_VIEW_MODEL_H
#define QUICK_LEDGER_EXPLORER_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QAbstractItemModel>
#include <QSortFilterProxyModel>
#include "LedgerEntriesModel.h"

// Coordinator for the Journal tab of the Ledger workspace. Owns a LedgerEntriesModel
// (behind a search proxy), an optional account scope, single-entry inspection, and a
// historical balance read. Every value is engine-derived (AuditJournal); nothing is
// written and no balance is cached or recomputed here.
class LedgerExplorerViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* entriesModel READ entriesModel CONSTANT)

    Q_PROPERTY(int     entryCount    READ entryCount    NOTIFY changed)
    Q_PROPERTY(int     filteredCount READ filteredCount NOTIFY filteredCountChanged)
    Q_PROPERTY(bool    hasScope         READ hasScope         NOTIFY changed)
    Q_PROPERTY(QString scopeAccountName READ scopeAccountName NOTIFY changed)
    Q_PROPERTY(QString scopeBalanceText READ scopeBalanceText NOTIFY changed)
    Q_PROPERTY(QString scopeSideText    READ scopeSideText    NOTIFY changed)

    Q_PROPERTY(QVariantMap currentEntry READ currentEntry NOTIFY currentEntryChanged)
    Q_PROPERTY(int         headSeq      READ headSeq      NOTIFY changed)

public:
    explicit LedgerExplorerViewModel(QObject* parent = nullptr);

    QAbstractItemModel* entriesModel() const;

    int     entryCount()    const;
    int     filteredCount() const;
    bool    hasScope()         const { return scopeAccountId_ != 0; }
    QString scopeAccountName() const { return scopeAccountName_; }
    QString scopeBalanceText() const { return scopeBalanceText_; }
    QString scopeSideText()    const { return scopeSideText_; }
    QVariantMap currentEntry() const { return currentEntry_; }
    int     headSeq()       const;

    Q_INVOKABLE void setAccountScope(int accountId);
    Q_INVOKABLE void setAccountScopeByName(const QString& name);   // resolve id, then scope
    Q_INVOKABLE void clearScope();
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void inspect(int entryId);
    Q_INVOKABLE QString balanceAtText(int accountId, int seq);   // historical balance, magnitude
    Q_INVOKABLE void refresh();

signals:
    void changed();
    void filteredCountChanged();
    void currentEntryChanged();

private:
    void recomputeScope();

    LedgerEntriesModel*    source_;
    QSortFilterProxyModel* proxy_;
    uint32_t               scopeAccountId_ = 0;
    QString                scopeAccountName_;
    QString                scopeBalanceText_;
    QString                scopeSideText_;
    QVariantMap            currentEntry_;
};

#endif // QUICK_LEDGER_EXPLORER_VIEW_MODEL_H
