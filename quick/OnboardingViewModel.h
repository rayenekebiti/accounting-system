#ifndef QUICK_ONBOARDING_VIEW_MODEL_H
#define QUICK_ONBOARDING_VIEW_MODEL_H

#include <QObject>
#include <QString>

// OnboardingViewModel — the first-run company profile wizard's backing model. It writes ONLY
// preferences (QSettings): business identity, currency, fiscal-year start, default language. It
// authors NO accounting events — settings stay strictly separate from accounting history (the
// invariant every other VM already respects). `needed()` is true only on a genuinely new, empty
// installation, so existing users are never forced back through it.
class OnboardingViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    needed        READ needed        CONSTANT)
    Q_PROPERTY(QString businessName  READ businessName  WRITE setBusinessName  NOTIFY changed)
    Q_PROPERTY(QString address       READ address       WRITE setAddress       NOTIFY changed)
    Q_PROPERTY(QString taxNumber     READ taxNumber     WRITE setTaxNumber     NOTIFY changed)
    Q_PROPERTY(QString currency      READ currency      WRITE setCurrency      NOTIFY changed)
    Q_PROPERTY(QString fiscalYearStart READ fiscalYearStart WRITE setFiscalYearStart NOTIFY changed)
    Q_PROPERTY(QString language      READ language      WRITE setLanguage      NOTIFY changed)
    Q_PROPERTY(bool    canComplete   READ canComplete   NOTIFY changed)

public:
    explicit OnboardingViewModel(QObject* parent = nullptr);

    // True only when onboarding was never completed AND the books are still empty (fresh install).
    bool needed() const;

    QString businessName()    const { return businessName_; }
    QString address()         const { return address_; }
    QString taxNumber()       const { return taxNumber_; }
    QString currency()        const { return currency_; }
    QString fiscalYearStart() const { return fiscalYearStart_; }   // "MM-DD" (e.g. 01-01)
    QString language()        const { return language_; }
    // A complete profile needs at least a business name (everything else has a sane default).
    bool    canComplete()     const { return !businessName_.trimmed().isEmpty(); }

    void setBusinessName(const QString& v);
    void setAddress(const QString& v);
    void setTaxNumber(const QString& v);
    void setCurrency(const QString& v);
    void setFiscalYearStart(const QString& v);
    void setLanguage(const QString& v);

    // Persist the profile to QSettings and mark onboarding complete. Returns false (and does not
    // mark complete) if the business name is empty. Authors NO accounting events.
    Q_INVOKABLE bool commit();
    // Explicit opt-out: mark onboarding complete WITHOUT a profile (skippable only on purpose).
    Q_INVOKABLE void skip();

signals:
    void changed();
    void finished();   // wizard should close and hand off to the main app

private:
    QString businessName_, address_, taxNumber_, currency_, fiscalYearStart_, language_;
};

#endif // QUICK_ONBOARDING_VIEW_MODEL_H
