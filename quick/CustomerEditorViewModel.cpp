#include "CustomerEditorViewModel.h"
#include "storage/StorageService.h"
#include "core/Money.h"
#include <QRegularExpression>
#include <stdexcept>

CustomerEditorViewModel::CustomerEditorViewModel(QObject* parent)
    : QObject(parent)
{}

// ── Setters ──────────────────────────────────────────────────────────────────

void CustomerEditorViewModel::setName(const QString& v)
{
    if (name_ == v) return;
    name_ = v;
    setDirty(true);
    revalidate();
    emit nameChanged();
}

void CustomerEditorViewModel::setEmail(const QString& v)
{
    if (email_ == v) return;
    email_ = v;
    setDirty(true);
    revalidate();
    emit emailChanged();
}

void CustomerEditorViewModel::setPhone(const QString& v)
{
    if (phone_ == v) return;
    phone_ = v;
    setDirty(true);
    revalidate();
    emit phoneChanged();
}

void CustomerEditorViewModel::setTaxNumber(const QString& v)
{
    if (taxNumber_ == v) return;
    taxNumber_ = v;
    setDirty(true);
    revalidate();
    emit taxNumberChanged();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CustomerEditorViewModel::beginNew()
{
    name_        = QString();
    email_       = QString();
    phone_       = QString();
    taxNumber_   = QString();
    balanceText_ = QStringLiteral("$0.00");
    editId_      = -1;
    isNew_       = true;
    dirty_       = false;
    showErrors_  = false;
    startingBalance_ = Money();

    revalidate();

    emit nameChanged();
    emit emailChanged();
    emit phoneChanged();
    emit taxNumberChanged();
    emit balanceChanged();
    emit isNewChanged();
    emit dirtyChanged();
    emit showErrorsChanged();
    emit validationChanged();
}

void CustomerEditorViewModel::beginEdit(int customerId)
{
    editId_     = customerId;
    isNew_      = false;
    dirty_      = false;
    showErrors_ = false;

    auto& storage = StorageService::instance();
    Customer c = storage.customers().load(static_cast<uint32_t>(customerId));

    name_      = QString::fromUtf8(c.getName());
    email_     = QString::fromUtf8(c.getEmail());
    phone_     = QString::fromUtf8(c.getPhone());
    taxNumber_ = QString::fromUtf8(c.getTaxNumber());

    // Preserve the stored starting balance so commit() doesn't zero it out
    startingBalance_ = c.getBalance();

    // Show the computed aggregate balance (single-pass O(n+m+p))
    auto agg = storage.computeCustomerAggregates();
    auto it  = agg.find(static_cast<uint32_t>(customerId));
    if (it != agg.end()) {
        balanceText_ = QString("$%1").arg(it->second.balance.toDouble(), 0, 'f', 2);
    } else {
        balanceText_ = QStringLiteral("$0.00");
    }

    revalidate();

    emit nameChanged();
    emit emailChanged();
    emit phoneChanged();
    emit taxNumberChanged();
    emit balanceChanged();
    emit isNewChanged();
    emit dirtyChanged();
    emit showErrorsChanged();
    emit validationChanged();
}

bool CustomerEditorViewModel::commit()
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
        // Hold QByteArray locals so constData() lifetime is guaranteed across
        // CustomerData construction — a temporary .toUtf8().constData() would dangle.
        const QByteArray nameUtf8      = name_.toUtf8();
        const QByteArray emailUtf8     = email_.toUtf8();
        const QByteArray phoneUtf8     = phone_.toUtf8();
        const QByteArray taxUtf8       = taxNumber_.toUtf8();

        CustomerData d;
        d.id        = 0;
        d.name      = nameUtf8.constData();
        d.email     = emailUtf8.constData();
        d.phone     = phoneUtf8.constData();
        d.taxNumber = taxUtf8.constData();
        // NEW: start at zero; EDIT: preserve the stored starting balance
        d.balance   = isNew_ ? Money() : startingBalance_;
        d.isDeleted = false;

        Customer c(d);

        // Event-authored: the authoritative event is the first committed fact; the
        // projection is updated by the journal's projector (never mutated directly).
        auto& storage = StorageService::instance();
        if (isNew_) {
            storage.audit().recordCustomerCreated(c, StorageService::now());
        } else {
            c.setId(static_cast<uint32_t>(editId_));
            storage.audit().recordCustomerUpdated(c, StorageService::now());
        }
    } catch (const std::exception& e) {
        emit saveFailed(QString::fromUtf8(e.what()));
        return false;
    }

    dirty_ = false;
    emit dirtyChanged();
    emit saved();
    return true;
}

void CustomerEditorViewModel::discard()
{
    emit discarded();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void CustomerEditorViewModel::revalidate()
{
    QString nameErr, emailErr, phoneErr, taxErr;

    // Error properties expose stable KEYS (not display text): QML maps each key
    // to a qsTr() message so translation happens at the presentation layer and
    // retranslates live. Limits are in BYTES (fixed UTF-8 storage buffers); a
    // char-count check would let a multibyte name (Arabic ≈ 2 B/char) overflow
    // and truncate mid-codepoint → mojibake.
    if (name_.isEmpty())
        nameErr = QStringLiteral("required");
    else if (name_.toUtf8().size() > 31)
        nameErr = QStringLiteral("tooLong");

    if (!email_.isEmpty()) {
        static const QRegularExpression emailRe(
            QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$"));
        if (!emailRe.match(email_).hasMatch())
            emailErr = QStringLiteral("invalidEmail");
        else if (email_.toUtf8().size() > 47)
            emailErr = QStringLiteral("tooLong");
    }

    if (phone_.toUtf8().size() > 15)
        phoneErr = QStringLiteral("tooLong");

    if (taxNumber_.toUtf8().size() > 15)
        taxErr = QStringLiteral("tooLong");

    const bool changed = (nameError_  != nameErr)
                      || (emailError_ != emailErr)
                      || (phoneError_ != phoneErr)
                      || (taxError_   != taxErr);

    nameError_  = nameErr;
    emailError_ = emailErr;
    phoneError_ = phoneErr;
    taxError_   = taxErr;

    if (changed) emit validationChanged();
}

void CustomerEditorViewModel::setDirty(bool v)
{
    if (dirty_ == v) return;
    dirty_ = v;
    emit dirtyChanged();
}

QString CustomerEditorViewModel::firstInvalidField() const
{
    if (!nameError_.isEmpty())  return QStringLiteral("name");
    if (!emailError_.isEmpty()) return QStringLiteral("email");
    if (!phoneError_.isEmpty()) return QStringLiteral("phone");
    if (!taxError_.isEmpty())   return QStringLiteral("taxNumber");
    return {};
}
