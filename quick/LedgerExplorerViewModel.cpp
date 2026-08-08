#include "LedgerExplorerViewModel.h"
#include "AccountsListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include <cstdlib>

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

LedgerExplorerViewModel::LedgerExplorerViewModel(QObject* parent)
    : QObject(parent)
    , source_(new LedgerEntriesModel(this))
    , proxy_(new QSortFilterProxyModel(this))
{
    source_->refresh();
    proxy_->setSourceModel(source_);
    proxy_->setFilterRole(LedgerEntriesModel::SearchTextRole);
    proxy_->setFilterCaseSensitivity(Qt::CaseInsensitive);

    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &LedgerExplorerViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &LedgerExplorerViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &LedgerExplorerViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &LedgerExplorerViewModel::filteredCountChanged);
}

QAbstractItemModel* LedgerExplorerViewModel::entriesModel() const { return proxy_; }

int LedgerExplorerViewModel::entryCount()    const { return static_cast<int>(source_->rows().size()); }
int LedgerExplorerViewModel::filteredCount() const { return proxy_->rowCount(); }

int LedgerExplorerViewModel::headSeq() const
{
    return StorageService::instance().isInitialized()
               ? static_cast<int>(StorageService::instance().audit().lastSeq()) : 0;
}

void LedgerExplorerViewModel::setAccountScope(int accountId)
{
    scopeAccountId_ = static_cast<uint32_t>(accountId);
    source_->setAccountFilter(scopeAccountId_);   // refreshes the source
    recomputeScope();
    emit changed();
    emit filteredCountChanged();
}

void LedgerExplorerViewModel::setAccountScopeByName(const QString& name)
{
    if (!StorageService::instance().isInitialized()) return;
    const int id = StorageService::instance().audit().accountIdByName(name.toStdString());
    if (id >= 0) setAccountScope(id);
}

void LedgerExplorerViewModel::clearScope()
{
    scopeAccountId_ = 0;
    source_->setAccountFilter(0);
    recomputeScope();
    emit changed();
    emit filteredCountChanged();
}

void LedgerExplorerViewModel::setSearchText(const QString& text)
{
    proxy_->setFilterFixedString(text);
    emit filteredCountChanged();
}

void LedgerExplorerViewModel::recomputeScope()
{
    scopeAccountName_.clear();
    scopeBalanceText_.clear();
    scopeSideText_.clear();
    if (scopeAccountId_ == 0 || !StorageService::instance().isInitialized()) return;

    auto& aj = StorageService::instance().audit();
    for (const auto& a : aj.listAccounts()) {
        if (a.id != scopeAccountId_) continue;
        scopeAccountName_ = QString::fromStdString(a.name);
        scopeBalanceText_ = money(std::llabs(a.balanceCents));
        scopeSideText_    = AccountsListModel::normalSide(a.type);
        break;
    }
}

void LedgerExplorerViewModel::inspect(int entryId)
{
    currentEntry_.clear();
    if (!StorageService::instance().isInitialized()) { emit currentEntryChanged(); return; }

    auto& aj = StorageService::instance().audit();
    const auto e = aj.entryById(static_cast<uint32_t>(entryId));
    if (e.postings.empty()) { emit currentEntryChanged(); return; }   // absent

    QHash<uint32_t, QString> names, types;
    for (const auto& a : aj.listAccounts()) {
        names.insert(a.id, QString::fromStdString(a.name));
        types.insert(a.id, AccountsListModel::typeName(a.type));
    }

    QVariantList postings;
    int64_t sum = 0, debitTotal = 0;
    for (const auto& p : e.postings) {
        sum += p.amountCents;
        if (p.amountCents > 0) debitTotal += p.amountCents;
        QVariantMap m;
        m.insert("accountId",  static_cast<int>(p.accountId));
        m.insert("account",    names.value(p.accountId, QStringLiteral("—")));
        m.insert("typeName",   types.value(p.accountId, QStringLiteral("—")));
        m.insert("debitText",  p.amountCents > 0 ? money(p.amountCents)  : QString());
        m.insert("creditText", p.amountCents < 0 ? money(-p.amountCents) : QString());
        postings.append(m);
    }

    currentEntry_.insert("id",         static_cast<int>(e.id));
    currentEntry_.insert("date",       QString::fromStdString(e.effectiveDate.toString()));
    currentEntry_.insert("isReversal", e.reverses   != 0xFFFFFFFFu);
    currentEntry_.insert("reverses",   e.reverses   != 0xFFFFFFFFu ? static_cast<int>(e.reverses)   : -1);
    currentEntry_.insert("isReversed", e.reversedBy != 0xFFFFFFFFu);
    currentEntry_.insert("reversedBy", e.reversedBy != 0xFFFFFFFFu ? static_cast<int>(e.reversedBy) : -1);
    currentEntry_.insert("totalText",  money(debitTotal));
    currentEntry_.insert("balanced",   sum == 0);
    currentEntry_.insert("postings",   postings);

    emit currentEntryChanged();
}

QString LedgerExplorerViewModel::balanceAtText(int accountId, int seq)
{
    if (!StorageService::instance().isInitialized()) return money(0);
    const int64_t b = StorageService::instance().audit()
                          .balanceAt(static_cast<uint32_t>(accountId), static_cast<uint64_t>(seq));
    return money(std::llabs(b));
}

void LedgerExplorerViewModel::refresh()
{
    source_->refresh();       // honours the current account scope
    recomputeScope();
    emit changed();
    emit filteredCountChanged();
}
