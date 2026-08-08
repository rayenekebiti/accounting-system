#include "PeriodCloseViewModel.h"

#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/IsoDate.h"

#include <stdexcept>

PeriodCloseViewModel::PeriodCloseViewModel(QObject* parent)
    : QObject(parent)
{}

int PeriodCloseViewModel::closedCount() const
{
    if (!StorageService::instance().isInitialized()) return 0;
    return static_cast<int>(StorageService::instance().audit().closedPeriodCount());
}

bool PeriodCloseViewModel::closePeriod(const QString& label, const QString& startIso, const QString& endIso)
{
    if (!StorageService::instance().isInitialized()) { emit actionFailed(QStringLiteral("notReady")); return false; }
    if (label.trimmed().isEmpty()) { emit actionFailed(QStringLiteral("labelRequired")); return false; }

    const auto start = IsoDate::fromString(startIso.toStdString());
    const auto end   = IsoDate::fromString(endIso.toStdString());
    if (!start.has_value() || !end.has_value()) { emit actionFailed(QStringLiteral("invalidDate")); return false; }

    try {
        StorageService::instance().audit().closePeriod(
            label.toStdString(), start.value(), end.value(), StorageService::now());
    } catch (const std::exception& e) {
        emit actionFailed(QString::fromUtf8(e.what()));
        return false;
    }
    emit changed();
    return true;
}

bool PeriodCloseViewModel::reopenPeriod(const QString& label)
{
    if (!StorageService::instance().isInitialized()) { emit actionFailed(QStringLiteral("notReady")); return false; }
    try {
        StorageService::instance().audit().reopenPeriod(label.toStdString(), StorageService::now());
    } catch (const std::exception& e) {
        emit actionFailed(QString::fromUtf8(e.what()));
        return false;
    }
    emit changed();
    return true;
}

bool PeriodCloseViewModel::isDateInClosedPeriod(const QString& iso) const
{
    if (!StorageService::instance().isInitialized()) return false;
    const auto d = IsoDate::fromString(iso.toStdString());
    if (!d.has_value()) return false;
    return StorageService::instance().audit().isDateInClosedPeriod(d.value());
}
