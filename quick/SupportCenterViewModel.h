#ifndef QUICK_SUPPORT_CENTER_VIEW_MODEL_H
#define QUICK_SUPPORT_CENTER_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>

// SupportCenterViewModel — backs Settings → Support Center. Lets the user file a problem report
// (stored LOCALLY as a support ticket), attach a privacy-safe diagnostics bundle (app health only —
// never accounting data), see their stable Support ID, and track ticket status. Reward eligibility is
// recorded but NEVER auto-applied. Everything is local; nothing is transmitted. Authors NO accounting
// events and never mutates the engine — it sits entirely above StorageService.
class SupportCenterViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString      supportId      READ supportId      CONSTANT)
    Q_PROPERTY(QVariantList tickets        READ tickets        NOTIFY changed)
    Q_PROPERTY(QString      lastBundlePath READ lastBundlePath NOTIFY changed)
    Q_PROPERTY(QString      lastResult     READ lastResult     NOTIFY changed)
    Q_PROPERTY(bool         busy           READ busy           NOTIFY changed)

public:
    explicit SupportCenterViewModel(QObject* parent = nullptr);

    QString      supportId() const;
    QVariantList tickets() const { return tickets_; }
    QString      lastBundlePath() const { return lastBundlePath_; }
    QString      lastResult() const { return lastResult_; }
    bool         busy() const { return busy_; }

    // File a report. Category/severity are untranslated KEYS (the QML maps them to localized labels).
    // If attachDiagnostics, a privacy-safe SupportBundle.zip is generated and linked to the ticket.
    // Returns the new ticket id ("TCK-…"), or empty on failure. Authors no accounting events.
    Q_INVOKABLE QString submitReport(const QString& category, const QString& severity,
                                     const QString& whatHappened, const QString& expected,
                                     const QString& steps, bool attachDiagnostics);

    // Generate a standalone diagnostics bundle (no ticket). Returns the written path.
    Q_INVOKABLE QString exportDiagnostics();

    // Operator/local ticket management (still fully local).
    Q_INVOKABLE void setStatus(const QString& id, const QString& statusKey);
    // Mark a ticket's feedback valuable → records a manual reward-eligibility note (no auto-discount).
    Q_INVOKABLE void markValuable(const QString& id);
    // The human-readable reward eligibility record for a ticket (empty if not marked valuable).
    Q_INVOKABLE QString rewardEligibility(const QString& id) const;

    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    QString supportDir() const;         // <dataDir>/support (or CWD if the engine isn't open)
    QString generateBundle();           // privacy-safe bundle → path

    QVariantList tickets_;
    QString      lastBundlePath_;
    QString      lastResult_;
    bool         busy_ = false;
};

#endif // QUICK_SUPPORT_CENTER_VIEW_MODEL_H
