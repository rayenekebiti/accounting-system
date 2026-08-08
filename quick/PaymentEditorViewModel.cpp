#include "PaymentEditorViewModel.h"
#include "storage/StorageService.h"
#include "core/Money.h"
#include "core/IsoDate.h"
#include <QVariantMap>
#include <QDate>
#include <cmath>
#include <stdexcept>

// Reject absurd magnitudes at the UI boundary well before Money's int64-cents range: $1
// trillion in major units is ~1e14 cents, far inside int64 (~9.2e18). Non-finite / larger
// values are meaningless input, not money.
static constexpr double kMaxAmountMajor = 1e12;

PaymentEditorViewModel::PaymentEditorViewModel(QObject* parent)
    : QObject(parent)
{}

void PaymentEditorViewModel::setCustomerId(int v)
{ if (customerId_ == v) return; customerId_ = v; setDirty(true); revalidate(); emit customerIdChanged(); }
void PaymentEditorViewModel::setDate(const QString& v)
{ if (date_ == v) return; date_ = v; setDirty(true); revalidate(); emit dateChanged(); }
void PaymentEditorViewModel::setAmount(const QString& v)
{ if (amount_ == v) return; amount_ = v; setDirty(true); revalidate(); emit amountChanged(); }

void PaymentEditorViewModel::beginNew()
{
    customerId_ = -1;
    date_ = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));   // default to today
    amount_ = QString();
    dirty_ = false; showErrors_ = false; lastPaymentId_ = -1;

    loadCustomerOptions();
    revalidate();
    emit customerIdChanged(); emit dateChanged(); emit amountChanged();
    emit dirtyChanged(); emit showErrorsChanged(); emit validationChanged();
}

bool PaymentEditorViewModel::commit()
{
    revalidate();
    if (!valid()) {
        showErrors_ = true;
        emit showErrorsChanged();
        emit validationChanged();
        emit validationFailed(firstInvalidField());
        return false;
    }

    try {
        auto& storage = StorageService::instance();
        const int64_t cents = Money::fromDouble(amount_.toDouble()).cents();
        const auto date = IsoDate::fromString(date_.toStdString());
        // AUTHORITY: the payment is an authoritative event — never a repository write.
        lastPaymentId_ = static_cast<int>(
            storage.audit().recordPayment(static_cast<uint32_t>(customerId_), cents,
                                          date.value(), StorageService::now()));
    } catch (const std::exception& e) {
        emit saveFailed(QString::fromUtf8(e.what()));
        return false;
    }

    dirty_ = false;
    emit dirtyChanged();
    emit savedChanged();
    emit saved();
    return true;
}

void PaymentEditorViewModel::discard() { emit discarded(); }

void PaymentEditorViewModel::revalidate()
{
    QString custErr, amtErr, dateErr;

    if (customerId_ < 0)
        custErr = QStringLiteral("required");

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

    const bool changed = (customerError_ != custErr) || (amountError_ != amtErr) || (dateError_ != dateErr);
    customerError_ = custErr; amountError_ = amtErr; dateError_ = dateErr;
    if (changed) emit validationChanged();
}

void PaymentEditorViewModel::setDirty(bool v)
{ if (dirty_ == v) return; dirty_ = v; emit dirtyChanged(); }

void PaymentEditorViewModel::loadCustomerOptions()
{
    customerOptions_.clear();
    if (StorageService::instance().isInitialized()) {
        for (const auto& c : StorageService::instance().customers().loadAll()) {
            QVariantMap m;
            m.insert(QStringLiteral("value"), static_cast<int>(c.getId()));
            m.insert(QStringLiteral("label"), QString::fromUtf8(c.getName()));
            customerOptions_.append(m);
        }
    }
    emit customerOptionsChanged();
}

QString PaymentEditorViewModel::firstInvalidField() const
{
    if (!customerError_.isEmpty()) return QStringLiteral("customer");
    if (!amountError_.isEmpty())   return QStringLiteral("amount");
    if (!dateError_.isEmpty())     return QStringLiteral("date");
    return {};
}
