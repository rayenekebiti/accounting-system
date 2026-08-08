#ifndef QUICK_EARLY_ACCESS_VIEW_MODEL_H
#define QUICK_EARLY_ACCESS_VIEW_MODEL_H

#include <QObject>
#include <QString>

// EarlyAccessViewModel — decides when the Early Access welcome notice appears, and remembers the
// user's choice. It is a pure preference state machine over QSettings (like the rest of the app's
// preferences); it authors NO accounting events and touches no engine state.
//
// Shows only when: first launch (never acknowledged), OR a MAJOR version update since the last
// acknowledgement. "Continue" acknowledges the current major (won't show again until the next major).
// "Remind me later" leaves state untouched (shows again next launch). "Don't show again" suppresses
// it permanently. The notice is also always available manually (About screen), regardless of state.
class EarlyAccessViewModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool    shouldShow READ shouldShow NOTIFY changed)   // show automatically at startup?
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit EarlyAccessViewModel(QObject* parent = nullptr);

    bool    shouldShow() const;
    QString appVersion() const;

    Q_INVOKABLE void acknowledge();     // "Continue" — mark the current major version as seen
    Q_INVOKABLE void remindLater();     // dismiss for this session; will reappear next launch
    Q_INVOKABLE void dontShowAgain();   // permanent suppression (still reachable manually)

signals:
    void changed();

private:
    int  currentMajor() const;
    int  acknowledgedMajor() const;     // -1 if never acknowledged
    bool suppressed() const;
};

#endif // QUICK_EARLY_ACCESS_VIEW_MODEL_H
