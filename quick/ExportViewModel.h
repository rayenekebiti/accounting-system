#ifndef QUICK_EXPORT_VIEW_MODEL_H
#define QUICK_EXPORT_VIEW_MODEL_H

#include <QObject>
#include <QString>

// ExportViewModel — the UI entry point for getting information out of Occountant (invoice to send a
// customer; trial balance / P&L / tax summary to hand an accountant). It writes portable CSV into
// <dataDir>/exports/ and reports the saved path. No accounting behaviour — it only reads the engine
// via ExportService. Kept thin + headless-drivable for regression tests.
class ExportViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString lastPath   READ lastPath   NOTIFY exported)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)

public:
    explicit ExportViewModel(QObject* parent = nullptr);

    QString lastPath()   const { return lastPath_; }
    QString statusText() const { return statusText_; }

    // Each returns the written file path (empty string on failure) and updates statusText.
    Q_INVOKABLE QString exportInvoice(int invoiceId);                     // CSV
    Q_INVOKABLE QString exportInvoicePdf(int invoiceId, const QString& lang);   // professional PDF (en/fr/ar)
    Q_INVOKABLE QString exportTrialBalance();
    Q_INVOKABLE QString exportIncomeStatement();
    Q_INVOKABLE QString exportTaxSummary();
    Q_INVOKABLE QString exportCustomerStatement(int customerId);          // charges/payments/running balance
    Q_INVOKABLE QString exportOutstandingSummary();                       // all customers + total outstanding
    // Reveal the exports folder in the OS file manager.
    Q_INVOKABLE bool openExportsFolder();

signals:
    void exported(const QString& path);
    void statusChanged();
    void exportFailed(const QString& reason);

private:
    QString exportsDir() const;   // <dataDir>/exports, created on demand
    QString finish(const QString& path, bool ok);
    QString lastPath_;
    QString statusText_;
};

#endif // QUICK_EXPORT_VIEW_MODEL_H
