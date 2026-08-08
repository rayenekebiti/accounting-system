#ifndef QUICK_PERIOD_CLOSE_VIEW_MODEL_H
#define QUICK_PERIOD_CLOSE_VIEW_MODEL_H

#include <QObject>
#include <QString>

// Period-close ViewModel — the missing UI wiring onto the EXISTING engine capability
// (AuditJournal::closePeriod / reopenPeriod). It adds no accounting behaviour: closing/reopening a
// period is already an authoritative, append-only event, and the engine already refuses corrections
// and voids whose effective date falls in a closed period. This VM only exposes that to the UI so a
// user can actually freeze a filed period. Reads are derived; writes go through the audit engine.
class PeriodCloseViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int closedCount READ closedCount NOTIFY changed)

public:
    explicit PeriodCloseViewModel(QObject* parent = nullptr);

    int closedCount() const;

    // Freeze [startIso, endIso] (inclusive, over effective dates) under `label`. Returns false and
    // emits actionFailed() on invalid dates or an engine error. Append-only + crash-safe.
    Q_INVOKABLE bool closePeriod(const QString& label, const QString& startIso, const QString& endIso);
    // Reopen a previously-closed period by label (append-only PeriodReopened event).
    Q_INVOKABLE bool reopenPeriod(const QString& label);
    // Membership test used by the UI to disable edit affordances for frozen dates.
    Q_INVOKABLE bool isDateInClosedPeriod(const QString& iso) const;

signals:
    void changed();
    void actionFailed(const QString& reason);

private:
    // (No cached state — every read is derived from the authoritative history.)
};

#endif // QUICK_PERIOD_CLOSE_VIEW_MODEL_H
