#ifndef QUICK_INVOICE_EDITOR_VIEW_MODEL_H
#define QUICK_INVOICE_EDITOR_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include "InvoiceDraftLinesModel.h"

class InvoiceEditorViewModel : public QObject
{
    Q_OBJECT

    // Header buffer props
    Q_PROPERTY(QString invoiceNumber READ invoiceNumber WRITE setInvoiceNumber NOTIFY invoiceNumberChanged)
    Q_PROPERTY(int     customerId    READ customerId    WRITE setCustomerId    NOTIFY customerIdChanged)
    Q_PROPERTY(QString issueDate     READ issueDate     WRITE setIssueDate     NOTIFY issueDateChanged)
    Q_PROPERTY(QString dueDate       READ dueDate       WRITE setDueDate       NOTIFY dueDateChanged)
    Q_PROPERTY(int     status        READ status        WRITE setStatus        NOTIFY statusChanged)

    // Read-only / derived
    Q_PROPERTY(InvoiceDraftLinesModel* lines    READ lines    CONSTANT)
    Q_PROPERTY(QVariantList customerOptions     READ customerOptions     NOTIFY customerOptionsChanged)
    Q_PROPERTY(QString subtotalText             READ subtotalText        NOTIFY totalsChanged)
    Q_PROPERTY(QString taxText                  READ taxText             NOTIFY totalsChanged)
    Q_PROPERTY(QString totalText                READ totalText           NOTIFY totalsChanged)
    Q_PROPERTY(bool    isNew                    READ isNew               NOTIFY isNewChanged)
    Q_PROPERTY(int     editId                   READ editId              NOTIFY isNewChanged)
    Q_PROPERTY(bool    dirty                    READ dirty               NOTIFY dirtyChanged)

    // Validation
    Q_PROPERTY(QString customerError    READ customerError    NOTIFY validationChanged)
    Q_PROPERTY(QString numberError      READ numberError      NOTIFY validationChanged)
    Q_PROPERTY(QString issueDateError   READ issueDateError   NOTIFY validationChanged)
    Q_PROPERTY(QString dueDateError     READ dueDateError     NOTIFY validationChanged)
    Q_PROPERTY(QString linesError       READ linesError       NOTIFY validationChanged)
    Q_PROPERTY(bool    valid            READ valid            NOTIFY validationChanged)
    Q_PROPERTY(bool    showErrors       READ showErrors       NOTIFY showErrorsChanged)

public:
    explicit InvoiceEditorViewModel(QObject* parent = nullptr);

    // Getters
    QString invoiceNumber() const { return invoiceNumber_; }
    int     customerId()    const { return customerId_; }
    QString issueDate()     const { return issueDate_; }
    QString dueDate()       const { return dueDate_; }
    int     status()        const { return status_; }

    InvoiceDraftLinesModel* lines() const { return lines_; }
    QVariantList customerOptions()  const { return customerOptions_; }
    QString subtotalText()          const;
    QString taxText()               const;
    QString totalText()             const;
    bool    isNew()                 const { return isNew_; }
    int     editId()                const { return editId_; }   // stable id when editing (−1 = new)
    bool    dirty()                 const { return dirty_; }

    QString customerError()         const { return customerError_; }
    QString numberError()           const { return numberError_; }
    QString issueDateError()        const { return issueDateError_; }
    QString dueDateError()          const { return dueDateError_; }
    QString linesError()            const { return linesError_; }
    bool    valid()                 const { return customerError_.isEmpty()
                                              && numberError_.isEmpty()
                                              && issueDateError_.isEmpty()
                                              && dueDateError_.isEmpty()
                                              && linesError_.isEmpty(); }
    bool    showErrors()            const { return showErrors_; }

    // Setters (set dirty + revalidate)
    void setInvoiceNumber(const QString& v);
    void setCustomerId(int v);
    void setIssueDate(const QString& v);
    void setDueDate(const QString& v);
    void setStatus(int v);

    // Lifecycle
    Q_INVOKABLE void beginNew();
    Q_INVOKABLE void beginEdit(int invoiceId);
    Q_INVOKABLE bool commit();
    Q_INVOKABLE void discard();
    // Call when the customer set changes (e.g. a customer was added) so the
    // cached dropdown options refresh; otherwise loadCustomerOptions() is a no-op.
    Q_INVOKABLE void refreshCustomerOptions();

signals:
    void invoiceNumberChanged();
    void customerIdChanged();
    void issueDateChanged();
    void dueDateChanged();
    void statusChanged();

    void customerOptionsChanged();
    void totalsChanged();
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
    void recomputeTotals();
    void setDirty(bool v);
    void loadCustomerOptions();
    QString firstInvalidField() const;
    static QString formatMoney(double v);

    InvoiceDraftLinesModel* lines_;

    QString invoiceNumber_;
    int     customerId_  = -1;
    QString issueDate_;
    QString dueDate_;
    int     status_      = 0; // INVOICE_DRAFT

    QVariantList customerOptions_;
    bool customerOptionsLoaded_ = false;

    bool isNew_       = true;
    bool dirty_       = false;
    bool showErrors_  = false;

    QString customerError_;
    QString numberError_;
    QString issueDateError_;
    QString dueDateError_;
    QString linesError_;

    QString peekedNumber_;
    int     editId_ = -1;

    // Helper: reset lines to empty without triggering dirty during beginNew
    void resetLinesQuiet();
};

#endif // QUICK_INVOICE_EDITOR_VIEW_MODEL_H
