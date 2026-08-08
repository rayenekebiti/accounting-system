#include "ExpenseEditorViewModel.h"
#include "ExpenseListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/Money.h"
#include "core/IsoDate.h"
#include "core/Expense.h"
#include "core/TaxCode.h"
#include <QVariantMap>
#include <QDate>
#include <cmath>
#include <string>
#include <stdexcept>

// Same UI-boundary magnitude cap as the other editors (see PaymentEditorViewModel).
static constexpr double kMaxAmountMajor = 1e12;

static QString money(int64_t cents) { return QString("$%1").arg(cents / 100.0, 0, 'f', 2); }

ExpenseEditorViewModel::ExpenseEditorViewModel(QObject* parent)
    : QObject(parent)
{}

void ExpenseEditorViewModel::setSupplierId(int v)
{ if (supplierId_ == v) return; supplierId_ = v; setDirty(true); emit supplierIdChanged(); }
void ExpenseEditorViewModel::setDate(const QString& v)
{ if (date_ == v) return; date_ = v; setDirty(true); revalidate(); emit dateChanged(); }
void ExpenseEditorViewModel::setAmount(const QString& v)
{ if (amount_ == v) return; amount_ = v; setDirty(true); revalidate(); emit amountChanged(); emit summaryChanged(); }
void ExpenseEditorViewModel::setCategory(int v)
{ if (category_ == v) return; category_ = v; setDirty(true); emit categoryChanged(); }
void ExpenseEditorViewModel::setPaymentMethod(int v)
{ if (paymentMethod_ == v) return; paymentMethod_ = v; setDirty(true); emit paymentMethodChanged(); }
void ExpenseEditorViewModel::setTaxCode(int v)
{
    if (taxCodeId_ == v) return;
    taxCodeId_ = v;
    taxRatePermille_ = 0;
    if (v >= 0 && StorageService::instance().isInitialized())
        taxRatePermille_ = StorageService::instance().audit().taxCodeById(static_cast<uint32_t>(v)).ratePermille;
    setDirty(true); emit taxCodeChanged(); emit summaryChanged();
}
void ExpenseEditorViewModel::setMemo(const QString& v)
{ if (memo_ == v) return; memo_ = v; setDirty(true); emit memoChanged(); }

QString ExpenseEditorViewModel::taxText() const
{
    bool ok = false; const double net = amount_.toDouble(&ok);
    if (!ok || !std::isfinite(net)) return money(0);
    return money(taxOnNet(Money::fromDouble(net).cents(), static_cast<int16_t>(taxRatePermille_)));
}

QString ExpenseEditorViewModel::totalText() const
{
    bool ok = false; const double net = amount_.toDouble(&ok);
    if (!ok || !std::isfinite(net)) return money(0);
    const int64_t n = Money::fromDouble(net).cents();
    return money(n + taxOnNet(n, static_cast<int16_t>(taxRatePermille_)));
}

void ExpenseEditorViewModel::loadTaxCodeOptions()
{
    taxCodeOptions_.clear();
    { QVariantMap m; m.insert(QStringLiteral("value"), -1);
      m.insert(QStringLiteral("label"), tr("No tax")); taxCodeOptions_.append(m); }
    if (StorageService::instance().isInitialized()) {
        for (const auto& c : StorageService::instance().audit().listTaxCodes()) {
            QVariantMap m;
            m.insert(QStringLiteral("value"), static_cast<int>(c.id));
            m.insert(QStringLiteral("label"),
                     QStringLiteral("%1 · %2 (%3%)").arg(QString::fromUtf8(c.name))
                         .arg(QString::fromUtf8(taxTypeName(c.type)))
                         .arg(c.ratePermille / 10.0, 0, 'f', 1));
            taxCodeOptions_.append(m);
        }
    }
    emit taxCodeOptionsChanged();
}

// Translated category label. categoryName() stays the English KEY (used for filtering + tone);
// the combo shows this localized text. (This VM is in the lupdate scan, so these tr() strings are
// extracted + translated.) retranslate() is wired to language changes for the reopened editor.
static QString categoryLabel(int c)
{
    switch (c) {
    case EXPENSE_CAT_OFFICE:    return ExpenseEditorViewModel::tr("Office");
    case EXPENSE_CAT_RENT:      return ExpenseEditorViewModel::tr("Rent");
    case EXPENSE_CAT_UTILITIES: return ExpenseEditorViewModel::tr("Utilities");
    case EXPENSE_CAT_TRAVEL:    return ExpenseEditorViewModel::tr("Travel");
    default:                    return ExpenseEditorViewModel::tr("Other");
    }
}

QVariantList ExpenseEditorViewModel::categoryOptions() const
{
    QVariantList out;
    for (int c = 0; c <= EXPENSE_CAT_OTHER; ++c) {
        QVariantMap m;
        m.insert(QStringLiteral("value"), c);
        m.insert(QStringLiteral("label"), categoryLabel(c));
        out.append(m);
    }
    return out;
}

void ExpenseEditorViewModel::beginNew()
{
    supplierId_ = -1;
    date_ = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));   // default to today
    amount_ = QString();
    category_ = EXPENSE_CAT_OTHER;
    paymentMethod_ = EXPENSE_PAY_CASH;
    taxCodeId_ = -1; taxRatePermille_ = 0;
    memo_ = QString();
    editingId_ = -1; oldAmountCents_ = 0; oldRatePermille_ = 0;
    dirty_ = false; showErrors_ = false; lastExpenseId_ = -1;

    loadSupplierOptions();
    loadTaxCodeOptions();
    revalidate();
    emit supplierIdChanged(); emit dateChanged(); emit amountChanged();
    emit categoryChanged(); emit paymentMethodChanged(); emit taxCodeChanged(); emit memoChanged();
    emit editingChanged(); emit dirtyChanged(); emit showErrorsChanged();
    emit validationChanged(); emit summaryChanged();
}

void ExpenseEditorViewModel::beginEdit(int expenseId)
{
    beginNew();
    if (!StorageService::instance().isInitialized()) return;
    Expense e = StorageService::instance().expenses().load(static_cast<uint32_t>(expenseId));
    editingId_     = expenseId;
    supplierId_    = e.getSupplierId() == EXPENSE_NO_SUPPLIER ? -1 : static_cast<int>(e.getSupplierId());
    date_          = QString::fromStdString(e.getDate().toString());
    oldAmountCents_ = e.getAmount().cents();
    amount_        = QString::number(oldAmountCents_ / 100.0, 'f', 2);
    category_      = e.getCategory();
    paymentMethod_ = e.getPaymentMethod();
    oldRatePermille_ = e.getTaxRatePermille();
    taxRatePermille_ = oldRatePermille_;
    // The expense stores the RATE, not a code id; surface the matching code for the picker.
    taxCodeId_ = -1;
    for (const auto& c : StorageService::instance().audit().listTaxCodes())
        if (c.ratePermille == taxRatePermille_) { taxCodeId_ = static_cast<int>(c.id); break; }
    memo_          = QString::fromUtf8(e.getMemo());
    dirty_ = false;

    revalidate();
    emit supplierIdChanged(); emit dateChanged(); emit amountChanged();
    emit categoryChanged(); emit paymentMethodChanged(); emit taxCodeChanged(); emit memoChanged();
    emit editingChanged(); emit dirtyChanged(); emit validationChanged(); emit summaryChanged();
}

bool ExpenseEditorViewModel::commit()
{
    revalidate();
    if (!valid()) {
        showErrors_ = true;
        emit showErrorsChanged();
        emit validationChanged();
        emit validationFailed(!amountError_.isEmpty() ? QStringLiteral("amount") : QStringLiteral("date"));
        return false;
    }

    try {
        auto& storage = StorageService::instance();
        const int64_t newNet = Money::fromDouble(amount_.toDouble()).cents();   // amount = net expense
        const int16_t rate   = static_cast<int16_t>(taxRatePermille_);
        const int64_t newTax = taxOnNet(newNet, rate);                          // recoverable input tax on top
        const auto date = IsoDate::fromString(date_.toStdString()).value();
        const bool correction = editingId_ >= 0;
        const int64_t oldNet = correction ? oldAmountCents_ : 0;
        const int64_t oldTax = correction ? taxOnNet(oldAmountCents_, static_cast<int16_t>(oldRatePermille_)) : 0;
        const int64_t netDelta = newNet - oldNet;
        const int64_t taxDelta = newTax - oldTax;
        const std::string memoStd = memo_.toStdString();

        ExpenseData d;
        d.id            = correction ? static_cast<uint32_t>(editingId_) : 0;
        d.supplierId    = supplierId_ < 0 ? EXPENSE_NO_SUPPLIER : static_cast<uint32_t>(supplierId_);
        d.amount        = Money::fromCents(newNet);
        d.date          = date;
        d.category      = static_cast<uint8_t>(category_);
        d.paymentMethod = static_cast<uint8_t>(paymentMethod_);
        d.status        = EXPENSE_ACTIVE;
        d.taxRatePermille = rate;
        d.memo          = memoStd.c_str();
        Expense e(d);
        if (correction) e.setId(static_cast<uint32_t>(editingId_));

        // AUTHORITY: the expense + its balanced ledger posting are ONE atomic authoritative fact.
        storage.audit().recordExpenseWithPosting(e, correction, netDelta, taxDelta, date, StorageService::now());
        lastExpenseId_ = static_cast<int>(e.getId());
    } catch (const std::exception& ex) {
        emit saveFailed(QString::fromUtf8(ex.what()));
        return false;
    }

    dirty_ = false;
    emit dirtyChanged();
    emit savedChanged();
    emit saved();
    return true;
}

bool ExpenseEditorViewModel::voidExpense(int expenseId)
{
    try {
        StorageService::instance().audit().recordExpenseVoided(
            static_cast<uint32_t>(expenseId), StorageService::now());
    } catch (const std::exception& ex) {
        emit saveFailed(QString::fromUtf8(ex.what()));
        return false;
    }
    emit saved();   // Main refreshes expenses + ledger views
    return true;
}

void ExpenseEditorViewModel::discard() { emit discarded(); }

void ExpenseEditorViewModel::revalidate()
{
    QString amtErr, dateErr;

    if (amount_.isEmpty()) {
        amtErr = QStringLiteral("required");
    } else {
        bool okNum = false;
        const double v = amount_.toDouble(&okNum);
        if (!okNum || !std::isfinite(v)) amtErr = QStringLiteral("invalidAmount");
        else if (v <= 0)                 amtErr = QStringLiteral("positiveAmount");
        else if (v > kMaxAmountMajor)    amtErr = QStringLiteral("amountTooLarge");
    }

    if (date_.isEmpty())
        dateErr = QStringLiteral("required");
    else if (!IsoDate::fromString(date_.toStdString()).has_value())
        dateErr = QStringLiteral("invalidDate");

    const bool changed = (amountError_ != amtErr) || (dateError_ != dateErr);
    amountError_ = amtErr; dateError_ = dateErr;
    if (changed) emit validationChanged();
}

void ExpenseEditorViewModel::setDirty(bool v)
{ if (dirty_ == v) return; dirty_ = v; emit dirtyChanged(); }

void ExpenseEditorViewModel::loadSupplierOptions()
{
    supplierOptions_.clear();
    // "No supplier" sentinel first (a cash expense may have no vendor record).
    { QVariantMap m; m.insert(QStringLiteral("value"), -1);
      m.insert(QStringLiteral("label"), tr("— none —")); supplierOptions_.append(m); }
    if (StorageService::instance().isInitialized()) {
        for (const auto& s : StorageService::instance().suppliers().loadAll()) {
            QVariantMap m;
            m.insert(QStringLiteral("value"), static_cast<int>(s.getId()));
            m.insert(QStringLiteral("label"), QString::fromUtf8(s.getName()));
            supplierOptions_.append(m);
        }
    }
    emit supplierOptionsChanged();
}
