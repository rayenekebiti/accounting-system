#include "AccountsViewModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

AccountsViewModel::AccountsViewModel(AccountsListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new AccountsFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &AccountsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &AccountsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &AccountsViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &AccountsViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* AccountsViewModel::listModel() const { return proxy_; }

int AccountsViewModel::totalCount()    const { return totalCount_; }
int AccountsViewModel::filteredCount() const { return proxy_->rowCount(); }

// Per-class magnitudes: debit-normal classes carry a positive Σ postings, credit-normal a
// negative one — display the natural (positive) magnitude of each.
QString AccountsViewModel::assetsText()      const { return money(assets_); }
QString AccountsViewModel::liabilitiesText() const { return money(liabilities_); }
QString AccountsViewModel::equityText()      const { return money(equity_); }
QString AccountsViewModel::incomeText()      const { return money(income_); }
QString AccountsViewModel::expenseText()     const { return money(expense_); }
QString AccountsViewModel::trialBalanceText() const { return money(trialBalance_); }
bool    AccountsViewModel::isBalanced()       const { return trialBalance_ == 0; }

void AccountsViewModel::setCategoryFilter(const QString& category) { proxy_->setCategoryFilter(category); }
void AccountsViewModel::setSearchText(const QString& text)         { proxy_->setSearchText(text); }

void AccountsViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
    emit filteredCountChanged();
}

void AccountsViewModel::recomputeSummaries()
{
    totalCount_ = 0;
    assets_ = liabilities_ = equity_ = income_ = expense_ = 0;

    for (const AccountsListModel::Row& r : source_->rows()) {
        ++totalCount_;
        const int64_t b = r.balanceCents;   // signed: debit +, credit −
        switch (r.type) {
        case AuditJournal::Asset:     assets_      += b;   break;
        case AuditJournal::Liability: liabilities_ += -b;  break;
        case AuditJournal::Equity:    equity_      += -b;  break;
        case AuditJournal::Income:    income_      += -b;  break;
        case AuditJournal::Expense:   expense_     += b;   break;
        default: break;
        }
    }

    // Authoritative invariant, straight from the engine (never recomputed here).
    trialBalance_ = StorageService::instance().isInitialized()
                        ? StorageService::instance().audit().trialBalanceTotal() : 0;
}
