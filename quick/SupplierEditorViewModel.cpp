#include "SupplierEditorViewModel.h"
#include "storage/StorageService.h"
#include "core/Money.h"
#include <QRegularExpression>
#include <stdexcept>

SupplierEditorViewModel::SupplierEditorViewModel(QObject* parent)
    : QObject(parent)
{}

// ── Setters ──────────────────────────────────────────────────────────────────
void SupplierEditorViewModel::setName(const QString& v)
{ if (name_ == v) return; name_ = v; setDirty(true); revalidate(); emit nameChanged(); }
void SupplierEditorViewModel::setEmail(const QString& v)
{ if (email_ == v) return; email_ = v; setDirty(true); revalidate(); emit emailChanged(); }
void SupplierEditorViewModel::setPhone(const QString& v)
{ if (phone_ == v) return; phone_ = v; setDirty(true); revalidate(); emit phoneChanged(); }
void SupplierEditorViewModel::setTaxNumber(const QString& v)
{ if (taxNumber_ == v) return; taxNumber_ = v; setDirty(true); revalidate(); emit taxNumberChanged(); }

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void SupplierEditorViewModel::beginNew()
{
    name_ = QString(); email_ = QString(); phone_ = QString(); taxNumber_ = QString();
    balanceText_ = QStringLiteral("$0.00");
    editId_ = -1; isNew_ = true; dirty_ = false; showErrors_ = false;
    startingBalance_ = Money();

    revalidate();
    emit nameChanged(); emit emailChanged(); emit phoneChanged(); emit taxNumberChanged();
    emit balanceChanged(); emit isNewChanged(); emit dirtyChanged(); emit showErrorsChanged();
    emit validationChanged();
}

void SupplierEditorViewModel::beginEdit(int supplierId)
{
    editId_ = supplierId; isNew_ = false; dirty_ = false; showErrors_ = false;

    auto& storage = StorageService::instance();
    Supplier s = storage.suppliers().load(static_cast<uint32_t>(supplierId));

    name_      = QString::fromUtf8(s.getName());
    email_     = QString::fromUtf8(s.getEmail());
    phone_     = QString::fromUtf8(s.getPhone());
    taxNumber_ = QString::fromUtf8(s.getTaxNumber());

    // Supplier balance is a stored payable (not invoice-derived) — preserve it across edit.
    startingBalance_ = s.getBalance();
    balanceText_ = QString("$%1").arg(startingBalance_.toDouble(), 0, 'f', 2);

    revalidate();
    emit nameChanged(); emit emailChanged(); emit phoneChanged(); emit taxNumberChanged();
    emit balanceChanged(); emit isNewChanged(); emit dirtyChanged(); emit showErrorsChanged();
    emit validationChanged();
}

bool SupplierEditorViewModel::commit()
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
        // Hold QByteArray locals so constData() outlives SupplierData construction.
        const QByteArray nameUtf8  = name_.toUtf8();
        const QByteArray emailUtf8 = email_.toUtf8();
        const QByteArray phoneUtf8 = phone_.toUtf8();
        const QByteArray taxUtf8   = taxNumber_.toUtf8();

        SupplierData d;
        d.id        = 0;
        d.name      = nameUtf8.constData();
        d.email     = emailUtf8.constData();
        d.phone     = phoneUtf8.constData();
        d.taxNumber = taxUtf8.constData();
        d.balance   = isNew_ ? Money() : startingBalance_;   // NEW: zero; EDIT: preserve
        d.isDeleted = false;

        Supplier s(d);

        // AUTHORITY: the authoritative event is the first committed fact; the projection is
        // updated by the journal's projector — never a direct repository write.
        auto& storage = StorageService::instance();
        if (isNew_) {
            storage.audit().recordSupplierCreated(s, StorageService::now());
        } else {
            s.setId(static_cast<uint32_t>(editId_));
            storage.audit().recordSupplierUpdated(s, StorageService::now());
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

void SupplierEditorViewModel::discard() { emit discarded(); }

// ── Private helpers ───────────────────────────────────────────────────────────
void SupplierEditorViewModel::revalidate()
{
    // Error properties expose stable KEYS (mapped to qsTr text in QML). Byte-length limits
    // match the fixed UTF-8 storage buffers (SUPPLIER_*_LENGTH). Identical rules to Customer.
    QString nameErr, emailErr, phoneErr, taxErr;

    if (name_.isEmpty())
        nameErr = QStringLiteral("required");
    else if (name_.toUtf8().size() > 31)
        nameErr = QStringLiteral("tooLong");

    if (!email_.isEmpty()) {
        static const QRegularExpression emailRe(QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$"));
        if (!emailRe.match(email_).hasMatch())
            emailErr = QStringLiteral("invalidEmail");
        else if (email_.toUtf8().size() > 47)
            emailErr = QStringLiteral("tooLong");
    }

    if (phone_.toUtf8().size() > 15)
        phoneErr = QStringLiteral("tooLong");
    if (taxNumber_.toUtf8().size() > 15)
        taxErr = QStringLiteral("tooLong");

    const bool changed = (nameError_ != nameErr) || (emailError_ != emailErr)
                      || (phoneError_ != phoneErr) || (taxError_ != taxErr);
    nameError_ = nameErr; emailError_ = emailErr; phoneError_ = phoneErr; taxError_ = taxErr;
    if (changed) emit validationChanged();
}

void SupplierEditorViewModel::setDirty(bool v)
{ if (dirty_ == v) return; dirty_ = v; emit dirtyChanged(); }

QString SupplierEditorViewModel::firstInvalidField() const
{
    if (!nameError_.isEmpty())  return QStringLiteral("name");
    if (!emailError_.isEmpty()) return QStringLiteral("email");
    if (!phoneError_.isEmpty()) return QStringLiteral("phone");
    if (!taxError_.isEmpty())   return QStringLiteral("taxNumber");
    return {};
}
