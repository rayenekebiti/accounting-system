#include "TaxCodeEditorViewModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/TaxCode.h"
#include "core/IsoDate.h"
#include <QVariantMap>
#include <QDate>
#include <cmath>

TaxCodeEditorViewModel::TaxCodeEditorViewModel(QObject* parent)
    : QObject(parent)
{}

void TaxCodeEditorViewModel::setType(int v)
{ if (type_ == v) return; type_ = v; setDirty(true); emit typeChanged(); }
void TaxCodeEditorViewModel::setName(const QString& v)
{ if (name_ == v) return; name_ = v; setDirty(true); revalidate(); emit nameChanged(); }
void TaxCodeEditorViewModel::setRate(const QString& v)
{ if (rate_ == v) return; rate_ = v; setDirty(true); revalidate(); emit rateChanged(); }
void TaxCodeEditorViewModel::setEffectiveDate(const QString& v)
{ if (effectiveDate_ == v) return; effectiveDate_ = v; setDirty(true); revalidate(); emit effectiveDateChanged(); }

QVariantList TaxCodeEditorViewModel::typeOptions() const
{
    QVariantList out;
    for (int t = 0; t <= TAX_TYPE_EXEMPT; ++t) {
        QVariantMap m;
        m.insert(QStringLiteral("value"), t);
        m.insert(QStringLiteral("label"), QString::fromUtf8(taxTypeName(static_cast<uint8_t>(t))));
        out.append(m);
    }
    return out;
}

void TaxCodeEditorViewModel::beginNew()
{
    type_ = TAX_TYPE_VAT;
    name_ = QString();
    rate_ = QString();
    effectiveDate_ = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    dirty_ = false; showErrors_ = false;
    revalidate();
    emit typeChanged(); emit nameChanged(); emit rateChanged(); emit effectiveDateChanged();
    emit dirtyChanged(); emit showErrorsChanged(); emit validationChanged();
}

bool TaxCodeEditorViewModel::commit()
{
    revalidate();
    if (!valid()) {
        showErrors_ = true;
        emit showErrorsChanged();
        emit validationChanged();
        return false;
    }
    try {
        auto& storage = StorageService::instance();
        // rate is a percentage; store per-mille (÷1000): 15.0% → 150‰. Zero-rated/Exempt → 0.
        int32_t ratePermille = 0;
        if (type_ != TAX_TYPE_ZERO_RATED && type_ != TAX_TYPE_EXEMPT)
            ratePermille = static_cast<int32_t>(std::llround(rate_.toDouble() * 10.0));
        const auto date = IsoDate::fromString(effectiveDate_.toStdString()).value();
        // AUTHORITY: a tax code is an authoritative append-only event, never a repository write.
        storage.audit().recordTaxCode(static_cast<uint8_t>(type_), name_.toStdString(),
                                      ratePermille, date, StorageService::now());
    } catch (const std::exception& e) {
        emit saveFailed(QString::fromUtf8(e.what()));
        return false;
    }
    dirty_ = false;
    emit dirtyChanged();
    emit saved();
    return true;
}

void TaxCodeEditorViewModel::discard() { emit discarded(); }

void TaxCodeEditorViewModel::revalidate()
{
    QString nameErr, rateErr, dateErr;

    if (name_.trimmed().isEmpty())
        nameErr = QStringLiteral("required");

    const bool zeroKind = (type_ == TAX_TYPE_ZERO_RATED || type_ == TAX_TYPE_EXEMPT);
    if (!zeroKind) {
        if (rate_.isEmpty()) {
            rateErr = QStringLiteral("required");
        } else {
            bool ok = false; const double v = rate_.toDouble(&ok);
            if (!ok || !std::isfinite(v)) rateErr = QStringLiteral("invalidRate");
            else if (v < 0 || v > 100)    rateErr = QStringLiteral("rangeRate");
        }
    }

    if (effectiveDate_.isEmpty())
        dateErr = QStringLiteral("required");
    else if (!IsoDate::fromString(effectiveDate_.toStdString()).has_value())
        dateErr = QStringLiteral("invalidDate");

    const bool changed = (nameError_ != nameErr) || (rateError_ != rateErr) || (dateError_ != dateErr);
    nameError_ = nameErr; rateError_ = rateErr; dateError_ = dateErr;
    if (changed) emit validationChanged();
}

void TaxCodeEditorViewModel::setDirty(bool v)
{ if (dirty_ == v) return; dirty_ = v; emit dirtyChanged(); }
