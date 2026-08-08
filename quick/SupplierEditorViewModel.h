#ifndef QUICK_SUPPLIER_EDITOR_VIEW_MODEL_H
#define QUICK_SUPPLIER_EDITOR_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include "core/Supplier.h"
#include "core/Money.h"

// Mirrors CustomerEditorViewModel. commit() routes through the authoritative event pipeline
// (AuditJournal::recordSupplierCreated / recordSupplierUpdated) — NEVER suppliers().save().
class SupplierEditorViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name       READ name       WRITE setName       NOTIFY nameChanged)
    Q_PROPERTY(QString email      READ email      WRITE setEmail      NOTIFY emailChanged)
    Q_PROPERTY(QString phone      READ phone      WRITE setPhone      NOTIFY phoneChanged)
    Q_PROPERTY(QString taxNumber  READ taxNumber  WRITE setTaxNumber  NOTIFY taxNumberChanged)

    Q_PROPERTY(QString balanceText  READ balanceText  NOTIFY balanceChanged)
    Q_PROPERTY(bool    isNew        READ isNew        NOTIFY isNewChanged)
    Q_PROPERTY(bool    dirty        READ dirty        NOTIFY dirtyChanged)

    Q_PROPERTY(QString nameError   READ nameError   NOTIFY validationChanged)
    Q_PROPERTY(QString emailError  READ emailError  NOTIFY validationChanged)
    Q_PROPERTY(QString phoneError  READ phoneError  NOTIFY validationChanged)
    Q_PROPERTY(QString taxError    READ taxError    NOTIFY validationChanged)
    Q_PROPERTY(bool    valid       READ valid       NOTIFY validationChanged)
    Q_PROPERTY(bool    showErrors  READ showErrors  NOTIFY showErrorsChanged)

public:
    explicit SupplierEditorViewModel(QObject* parent = nullptr);

    QString name()        const { return name_; }
    QString email()       const { return email_; }
    QString phone()       const { return phone_; }
    QString taxNumber()   const { return taxNumber_; }
    QString balanceText() const { return balanceText_; }
    bool    isNew()       const { return isNew_; }
    bool    dirty()       const { return dirty_; }

    QString nameError()  const { return nameError_; }
    QString emailError() const { return emailError_; }
    QString phoneError() const { return phoneError_; }
    QString taxError()   const { return taxError_; }
    bool    valid()      const {
        return nameError_.isEmpty() && emailError_.isEmpty()
            && phoneError_.isEmpty() && taxError_.isEmpty();
    }
    bool    showErrors() const { return showErrors_; }

    void setName(const QString& v);
    void setEmail(const QString& v);
    void setPhone(const QString& v);
    void setTaxNumber(const QString& v);

    Q_INVOKABLE void beginNew();
    Q_INVOKABLE void beginEdit(int supplierId);
    Q_INVOKABLE bool commit();
    Q_INVOKABLE void discard();

signals:
    void nameChanged();
    void emailChanged();
    void phoneChanged();
    void taxNumberChanged();
    void balanceChanged();
    void isNewChanged();
    void dirtyChanged();
    void showErrorsChanged();
    void validationChanged();

    void saved();
    void discarded();
    void saveFailed(const QString& message);
    void validationFailed(const QString& firstField);

private:
    void revalidate();
    void setDirty(bool v);
    QString firstInvalidField() const;

    QString name_;
    QString email_;
    QString phone_;
    QString taxNumber_;
    QString balanceText_ = QStringLiteral("$0.00");

    bool    isNew_      = true;
    bool    dirty_      = false;
    bool    showErrors_ = false;

    int     editId_ = -1;
    Money   startingBalance_;

    QString nameError_;
    QString emailError_;
    QString phoneError_;
    QString taxError_;
};

#endif // QUICK_SUPPLIER_EDITOR_VIEW_MODEL_H
