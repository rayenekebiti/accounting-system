#include "ExpensesViewModel.h"
#include "core/Expense.h"

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

ExpensesViewModel::ExpensesViewModel(ExpenseListModel* source, QObject* parent)
    : QObject(parent)
    , source_(source)
    , proxy_(new ExpenseFilterProxy(this))
{
    proxy_->setSourceModel(source_);

    connect(proxy_, &QAbstractItemModel::rowsInserted,  this, &ExpensesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::rowsRemoved,   this, &ExpensesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::modelReset,    this, &ExpensesViewModel::filteredCountChanged);
    connect(proxy_, &QAbstractItemModel::layoutChanged, this, &ExpensesViewModel::filteredCountChanged);

    recomputeSummaries();
}

QAbstractItemModel* ExpensesViewModel::listModel() const { return proxy_; }

int     ExpensesViewModel::totalCount()     const { return expenseCount_; }
int     ExpensesViewModel::filteredCount()  const { return proxy_->rowCount(); }
int     ExpensesViewModel::expenseCount()   const { return expenseCount_; }
QString ExpensesViewModel::totalSpentText() const { return money(netSpent_); }
QString ExpensesViewModel::cashText()       const { return money(cashSpent_); }
QString ExpensesViewModel::creditText()     const { return money(creditSpent_); }
int     ExpensesViewModel::voidCount()      const { return voidCount_; }

void ExpensesViewModel::setCategoryFilter(const QString& category) { proxy_->setCategoryFilter(category); }
void ExpensesViewModel::setSearchText(const QString& text)         { proxy_->setSearchText(text); }

void ExpensesViewModel::refresh()
{
    source_->refresh();
    recomputeSummaries();
    emit summaryChanged();
    emit filteredCountChanged();
}

void ExpensesViewModel::recomputeSummaries()
{
    expenseCount_ = 0;
    voidCount_    = 0;
    netSpent_ = cashSpent_ = creditSpent_ = 0;

    for (const ExpenseListModel::Row& r : source_->rows()) {
        ++expenseCount_;
        if (r.status == 1) { ++voidCount_; continue; }   // voided → excluded from spend totals
        netSpent_ += r.amountCents;                       // reversal rows are negative → net
        if (r.paymentMethod == EXPENSE_PAY_CREDIT) creditSpent_ += r.amountCents;
        else                                       cashSpent_   += r.amountCents;
    }
}
