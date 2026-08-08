#ifndef QUICK_TAX_CODE_EDITOR_VIEW_MODEL_H
#define QUICK_TAX_CODE_EDITOR_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>

// Authors a tax code through the authoritative engine (AuditJournal::recordTaxCode) — an
// append-only policy fact, never a repository write. Re-using an existing name records a new
// VERSION of that family (effective-dated); the rate on historical transactions is unaffected.
class TaxCodeEditorViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int     type          READ type          WRITE setType          NOTIFY typeChanged)
    Q_PROPERTY(QString name          READ name          WRITE setName          NOTIFY nameChanged)
    Q_PROPERTY(QString rate          READ rate          WRITE setRate          NOTIFY rateChanged)
    Q_PROPERTY(QString effectiveDate READ effectiveDate WRITE setEffectiveDate NOTIFY effectiveDateChanged)

    Q_PROPERTY(QVariantList typeOptions READ typeOptions CONSTANT)

    Q_PROPERTY(bool    dirty      READ dirty      NOTIFY dirtyChanged)
    Q_PROPERTY(QString nameError  READ nameError  NOTIFY validationChanged)
    Q_PROPERTY(QString rateError  READ rateError  NOTIFY validationChanged)
    Q_PROPERTY(QString dateError  READ dateError  NOTIFY validationChanged)
    Q_PROPERTY(bool    valid      READ valid      NOTIFY validationChanged)
    Q_PROPERTY(bool    showErrors READ showErrors NOTIFY showErrorsChanged)

public:
    explicit TaxCodeEditorViewModel(QObject* parent = nullptr);

    int          type()          const { return type_; }
    QString      name()          const { return name_; }
    QString      rate()          const { return rate_; }
    QString      effectiveDate() const { return effectiveDate_; }
    QVariantList typeOptions()   const;
    bool         dirty()         const { return dirty_; }

    QString nameError() const { return nameError_; }
    QString rateError() const { return rateError_; }
    QString dateError() const { return dateError_; }
    bool    valid()     const { return nameError_.isEmpty() && rateError_.isEmpty() && dateError_.isEmpty(); }
    bool    showErrors() const { return showErrors_; }

    void setType(int v);
    void setName(const QString& v);
    void setRate(const QString& v);
    void setEffectiveDate(const QString& v);

    Q_INVOKABLE void beginNew();
    Q_INVOKABLE bool commit();
    Q_INVOKABLE void discard();

signals:
    void typeChanged();
    void nameChanged();
    void rateChanged();
    void effectiveDateChanged();
    void dirtyChanged();
    void showErrorsChanged();
    void validationChanged();
    void saved();
    void discarded();
    void saveFailed(const QString& message);

private:
    void revalidate();
    void setDirty(bool v);

    int     type_ = 0;   // VAT
    QString name_;
    QString rate_;
    QString effectiveDate_;
    bool    dirty_      = false;
    bool    showErrors_ = false;
    QString nameError_, rateError_, dateError_;
};

#endif // QUICK_TAX_CODE_EDITOR_VIEW_MODEL_H
