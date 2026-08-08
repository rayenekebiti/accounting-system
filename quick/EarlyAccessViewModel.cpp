#include "EarlyAccessViewModel.h"
#include "app/AppInfo.h"

#include <QSettings>

namespace {
constexpr auto kAckMajorKey   = "earlyAccess/acknowledgedMajor";
constexpr auto kSuppressedKey = "earlyAccess/suppressed";
}

EarlyAccessViewModel::EarlyAccessViewModel(QObject* parent) : QObject(parent) {}

int EarlyAccessViewModel::currentMajor() const { return appinfo::kVersionMajor; }

int EarlyAccessViewModel::acknowledgedMajor() const
{
    QSettings s;
    return s.value(QLatin1String(kAckMajorKey), -1).toInt();   // -1 = never acknowledged
}

bool EarlyAccessViewModel::suppressed() const
{
    QSettings s;
    return s.value(QLatin1String(kSuppressedKey), false).toBool();
}

bool EarlyAccessViewModel::shouldShow() const
{
    if (suppressed()) return false;
    // First launch (-1) or a major-version bump since last acknowledgement.
    return acknowledgedMajor() < currentMajor();
}

QString EarlyAccessViewModel::appVersion() const { return appinfo::version(); }

void EarlyAccessViewModel::acknowledge()
{
    QSettings s;
    s.setValue(QLatin1String(kAckMajorKey), currentMajor());
    s.sync();
    emit changed();
}

void EarlyAccessViewModel::remindLater()
{
    // Intentionally persists nothing — shouldShow() stays true, so it reappears next launch.
    emit changed();
}

void EarlyAccessViewModel::dontShowAgain()
{
    QSettings s;
    s.setValue(QLatin1String(kSuppressedKey), true);
    s.sync();
    emit changed();
}
