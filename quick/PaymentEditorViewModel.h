#ifndef QUICK_PAYMENT_EDITOR_VIEW_MODEL_H
#define QUICK_PAYMENT_EDITOR_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>

// Records a payment through the authoritative settlement engine (AuditJournal::recordPayment).
// NOTE: PaymentRecorded has no reference/notes field — persisting one would be a storage-format
// change (out of scope). Reference/notes is therefore not offered here (documented limitation).
class PaymentEditorViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int     customerId  READ customerId  WRITE setCustomerId  NOTIFY customerIdChanged)
    Q_PROPERTY(QString date        READ date        WRITE setDate        NOTIFY dateChanged)
    Q_PROPERTY(QString amount      READ amount      WRITE setAmount      NOTIFY amountChanged)

    Q_PROPERTY(QVariantList customerOptions READ customerOptions NOTIFY customerOptionsChanged)

    Q_PROPERTY(bool    dirty       READ dirty       NOTIFY dirtyChanged)
    Q_PROPERTY(int     lastPaymentId READ lastPaymentId NOTIFY savedChanged)

    Q_PROPERTY(QString customerError READ customerError NOTIFY validationChanged)
    Q_PROPERTY(QString amountError   READ amountError   NOTIFY validationChanged)
    Q_PROPERTY(QString dateError     READ dateError     NOTIFY validationChanged)
    Q_PROPERTY(bool    valid         READ valid         NOTIFY validationChanged)
    Q_PROPERTY(bool    showErrors    READ showErrors    NOTIFY showErrorsChanged)

public:
    explicit PaymentEditorViewModel(QObject* parent = nullptr);

    int          customerId() const { return customerId_; }
    QString      date()       const { return date_; }
    QString      amount()     const { return amount_; }
    QVariantList customerOptions() const { return customerOptions_; }
    bool         dirty()      const { return dirty_; }
    int          lastPaymentId() const { return lastPaymentId_; }

    QString customerError() const { return customerError_; }
    QString amountError()   const { return amountError_; }
    QString dateError()     const { return dateError_; }
    bool    valid()         const { return customerError_.isEmpty() && amountError_.isEmpty() && dateError_.isEmpty(); }
    bool    showErrors()    const { return showErrors_; }

    void setCustomerId(int v);
    void setDate(const QString& v);
    void setAmount(const QString& v);

    Q_INVOKABLE void beginNew();
    Q_INVOKABLE bool commit();     // records the payment; on success lastPaymentId is the new id
    Q_INVOKABLE void discard();

signals:
    void customerIdChanged();
    void dateChanged();
    void amountChanged();
    void customerOptionsChanged();
    void dirtyChanged();
    void savedChanged();
    void showErrorsChanged();
    void validationChanged();

    void saved();
    void discarded();
    void saveFailed(const QString& message);
    void validationFailed(const QString& firstField);

private:
    void revalidate();
    void setDirty(bool v);
    void loadCustomerOptions();
    QString firstInvalidField() const;

    int     customerId_ = -1;
    QString date_;
    QString amount_;
    QVariantList customerOptions_;

    bool    dirty_       = false;
    bool    showErrors_  = false;
    int     lastPaymentId_ = -1;

    QString customerError_;
    QString amountError_;
    QString dateError_;
};

#endif // QUICK_PAYMENT_EDITOR_VIEW_MODEL_H
