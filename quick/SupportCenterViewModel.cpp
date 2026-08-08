#include "SupportCenterViewModel.h"

#include "app/SupportTicket.h"
#include "app/SupportTicketStore.h"
#include "app/SupportId.h"
#include "app/SupportBundle.h"
#include "storage/StorageService.h"

#include <QDir>
#include <QVariantMap>

using support::SupportTicket;
using support::SupportTicketStore;

SupportCenterViewModel::SupportCenterViewModel(QObject* parent) : QObject(parent)
{
    refresh();
}

QString SupportCenterViewModel::supportId() const { return supportid::get(); }

QString SupportCenterViewModel::supportDir() const
{
    const QString base = StorageService::instance().isInitialized()
        ? QString::fromStdString(StorageService::instance().dataDir())
        : QDir::currentPath();
    return base + "/support";
}

QString SupportCenterViewModel::generateBundle()
{
    supportbundle::Params p;
    if (StorageService::instance().isInitialized())
        p.dataDir = QString::fromStdString(StorageService::instance().dataDir());
    p.reason    = QStringLiteral("support-center");
    p.supportId = supportid::get();
    return supportbundle::generate(p);   // privacy-safe: allowlist keeps accounting data out
}

QString SupportCenterViewModel::submitReport(const QString& category, const QString& severity,
                                             const QString& whatHappened, const QString& expected,
                                             const QString& steps, bool attachDiagnostics)
{
    busy_ = true; emit changed();

    SupportTicket t;
    t.category     = category.isEmpty() ? QStringLiteral("Other") : category;
    t.severity     = severity.isEmpty() ? QStringLiteral("Minor") : severity;
    t.whatHappened = whatHappened;
    t.expected     = expected;
    t.steps        = steps;
    t.supportId    = supportid::get();

    SupportTicketStore store(supportDir());
    const QString id = store.add(t);

    if (!id.isEmpty() && attachDiagnostics) {
        const QString bundle = generateBundle();
        if (!bundle.isEmpty()) { store.setBundlePath(id, bundle); lastBundlePath_ = bundle; }
    }

    lastResult_ = id.isEmpty() ? QStringLiteral("error") : id;
    busy_ = false;
    refresh();
    return id;
}

QString SupportCenterViewModel::exportDiagnostics()
{
    busy_ = true; emit changed();
    const QString path = generateBundle();
    lastBundlePath_ = path;
    busy_ = false;
    emit changed();
    return path;
}

void SupportCenterViewModel::setStatus(const QString& id, const QString& statusKey)
{
    SupportTicketStore store(supportDir());
    store.setStatus(id, support::statusFromKey(statusKey));
    refresh();
}

void SupportCenterViewModel::markValuable(const QString& id)
{
    // Framework only: record eligibility. NO discount is ever applied automatically — a human decides.
    SupportTicketStore store(supportDir());
    store.markValuable(id, QStringLiteral("Eligible: 19% discount for 6 months (manual grant pending)"));
    refresh();
}

QString SupportCenterViewModel::rewardEligibility(const QString& id) const
{
    SupportTicketStore store(supportDir());
    SupportTicket t;
    if (!store.getById(id, t) || !t.valuable) return {};
    return QStringLiteral("%1 · %2 · %3").arg(supportid::get(), t.id, t.rewardNote);
}

void SupportCenterViewModel::refresh()
{
    tickets_.clear();
    SupportTicketStore store(supportDir());
    for (const SupportTicket& t : store.all()) {
        QVariantMap m;
        m["id"]           = t.id;
        m["category"]     = t.category;        // key; QML localizes for display
        m["severity"]     = t.severity;        // key
        m["status"]       = QString::fromLatin1(support::statusKey(t.status));   // key
        m["whatHappened"] = t.whatHappened;
        m["createdIso"]   = t.createdIso;
        m["valuable"]     = t.valuable;
        m["rewardNote"]   = t.rewardNote;
        m["hasBundle"]    = !t.bundlePath.isEmpty();
        tickets_.push_back(m);
    }
    emit changed();
}
