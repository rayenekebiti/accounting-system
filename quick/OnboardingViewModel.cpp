#include "OnboardingViewModel.h"
#include "storage/StorageService.h"

#include <QSettings>

OnboardingViewModel::OnboardingViewModel(QObject* parent) : QObject(parent)
{
    QSettings s;
    // Pre-fill from any existing prefs so re-running the wizard is non-destructive.
    businessName_    = s.value(QStringLiteral("company/name")).toString();
    address_         = s.value(QStringLiteral("company/address")).toString();
    taxNumber_       = s.value(QStringLiteral("company/taxId")).toString();
    currency_        = s.value(QStringLiteral("general/currency"), QStringLiteral("$")).toString();
    fiscalYearStart_ = s.value(QStringLiteral("company/fiscalYearStart"), QStringLiteral("01-01")).toString();
    language_        = s.value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
}

bool OnboardingViewModel::needed() const
{
    if (QSettings().value(QStringLiteral("onboarding/completed"), false).toBool())
        return false;   // already finished (or explicitly skipped) once
    // Only greet a genuinely EMPTY installation — never force an existing user back through it.
    auto& st = StorageService::instance();
    if (!st.isInitialized()) return true;
    const bool emptyBooks = st.customers().loadAll().empty() && st.invoices().loadAll().empty();
    return emptyBooks;
}

void OnboardingViewModel::setBusinessName(const QString& v)   { if (businessName_ == v) return;   businessName_ = v; emit changed(); }
void OnboardingViewModel::setAddress(const QString& v)        { if (address_ == v) return;        address_ = v; emit changed(); }
void OnboardingViewModel::setTaxNumber(const QString& v)      { if (taxNumber_ == v) return;      taxNumber_ = v; emit changed(); }
void OnboardingViewModel::setCurrency(const QString& v)       { if (currency_ == v) return;       currency_ = v; emit changed(); }
void OnboardingViewModel::setFiscalYearStart(const QString& v){ if (fiscalYearStart_ == v) return; fiscalYearStart_ = v; emit changed(); }
void OnboardingViewModel::setLanguage(const QString& v)       { if (language_ == v) return;       language_ = v; emit changed(); }

bool OnboardingViewModel::commit()
{
    if (!canComplete()) return false;   // a company profile needs at least a name
    QSettings s;
    // PREFERENCES ONLY — no accounting events. Same keys the Settings screen uses, so the profile
    // is immediately visible everywhere (exports, invoice documents) without a second source.
    s.setValue(QStringLiteral("company/name"),            businessName_.trimmed());
    s.setValue(QStringLiteral("company/address"),         address_);
    s.setValue(QStringLiteral("company/taxId"),           taxNumber_);
    s.setValue(QStringLiteral("general/currency"),        currency_.isEmpty() ? QStringLiteral("$") : currency_);
    s.setValue(QStringLiteral("company/fiscalYearStart"), fiscalYearStart_.isEmpty() ? QStringLiteral("01-01") : fiscalYearStart_);
    s.setValue(QStringLiteral("ui/language"),             language_.isEmpty() ? QStringLiteral("en") : language_);
    s.setValue(QStringLiteral("onboarding/completed"),    true);
    s.sync();
    emit finished();
    return true;
}

void OnboardingViewModel::skip()
{
    QSettings s;
    s.setValue(QStringLiteral("onboarding/completed"), true);
    s.setValue(QStringLiteral("onboarding/skipped"),   true);
    s.sync();
    emit finished();
}
