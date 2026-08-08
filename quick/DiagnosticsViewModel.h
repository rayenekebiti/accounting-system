#ifndef QUICK_DIAGNOSTICS_VIEW_MODEL_H
#define QUICK_DIAGNOSTICS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <cstdint>

// Read-only production diagnostics — every value comes straight from the authoritative engine
// (StorageService / AuditJournal) or the on-disk data directory. Nothing here mutates state.
class DiagnosticsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString engineVersion   READ engineVersion   NOTIFY changed)
    Q_PROPERTY(QString compatVersion   READ compatVersion   NOTIFY changed)
    Q_PROPERTY(QString postingPolicy   READ postingPolicy   NOTIFY changed)
    Q_PROPERTY(QString compatStatus    READ compatStatus    NOTIFY changed)
    Q_PROPERTY(QString snapshotStatus  READ snapshotStatus  NOTIFY changed)
    Q_PROPERTY(QString databaseSize    READ databaseSize    NOTIFY changed)
    Q_PROPERTY(QString eventCount      READ eventCount      NOTIFY changed)
    Q_PROPERTY(QString currentSeq      READ currentSeq      NOTIFY changed)
    Q_PROPERTY(QString accountCount    READ accountCount    NOTIFY changed)
    Q_PROPERTY(QString trialBalance    READ trialBalance    NOTIFY changed)
    Q_PROPERTY(bool    trialBalanceOk  READ trialBalanceOk  NOTIFY changed)
    Q_PROPERTY(QString lastBackup      READ lastBackup      NOTIFY changed)

    // Support: a stable, non-PII install id the user can quote to support, and a one-click
    // diagnostics bundle (no accounting data) they can send. Nothing is transmitted automatically.
    Q_PROPERTY(QString supportId       READ supportId       CONSTANT)
    Q_PROPERTY(QString lastBundlePath  READ lastBundlePath  NOTIFY bundleChanged)
    Q_PROPERTY(bool    bundleBusy      READ bundleBusy      NOTIFY bundleChanged)

    // Verification results (populated by runVerification()).
    Q_PROPERTY(QString projectionResult READ projectionResult NOTIFY verificationChanged)
    Q_PROPERTY(bool    projectionOk      READ projectionOk     NOTIFY verificationChanged)
    Q_PROPERTY(QString replayResult      READ replayResult     NOTIFY verificationChanged)
    Q_PROPERTY(bool    replayOk          READ replayOk         NOTIFY verificationChanged)
    Q_PROPERTY(bool    verifying         READ verifying        NOTIFY verificationChanged)

public:
    explicit DiagnosticsViewModel(QObject* parent = nullptr);

    QString engineVersion()  const;
    QString compatVersion()  const;
    QString postingPolicy()  const;
    QString compatStatus()   const;
    QString snapshotStatus() const;
    QString databaseSize()   const;
    QString eventCount()     const;
    QString currentSeq()     const;
    QString accountCount()   const;
    QString trialBalance()   const;
    bool    trialBalanceOk() const;
    QString lastBackup()     const;

    QString supportId()      const;
    QString lastBundlePath() const { return lastBundlePath_; }
    bool    bundleBusy()     const { return bundleBusy_; }

    QString projectionResult() const;   // formatted from state so it retranslates live
    bool    projectionOk()     const { return projectionOk_; }
    QString replayResult()     const;   // formatted from state so it retranslates live
    bool    replayOk()         const { return replayOk_; }
    bool    verifying()        const { return verifying_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void runVerification();   // deep projection + replay-equivalence gate (non-destructive)
    // Compose a SupportBundle.zip (diagnostics only — no accounting data) under <dataDir>/support/.
    // Returns the written path (empty on failure). Nothing is transmitted; the user sends it manually.
    Q_INVOKABLE QString exportSupportBundle();
    // Re-emit so QML re-reads every getter under the newly-installed translator (live language switch).
    Q_INVOKABLE void retranslate() { emit changed(); emit verificationChanged(); }

signals:
    void changed();
    void verificationChanged();
    void bundleChanged();

private:
    bool     projectionOk_ = false;
    uint64_t projSeq_ = 0;
    bool     replayOk_ = false;
    uint64_t replaySeq_ = 0;
    QString  replayDetail_;
    bool     verifying_ = false;
    bool     ran_ = false;
    QString  lastBundlePath_;
    bool     bundleBusy_ = false;
};

#endif // QUICK_DIAGNOSTICS_VIEW_MODEL_H
