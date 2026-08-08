#include "SupportTicketStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QUuid>
#include <algorithm>

namespace support {

SupportTicketStore::SupportTicketStore(QString dir)
    : dir_(std::move(dir)), path_(dir_ + "/tickets.json")
{}

bool SupportTicketStore::load(QVector<SupportTicket>& out) const
{
    out.clear();
    QFile f(path_);
    if (!f.exists()) return true;           // no file yet is a valid empty store
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray raw = f.readAll();
    f.close();
    const QJsonDocument d = QJsonDocument::fromJson(raw);
    if (!d.isArray()) return false;
    for (const QJsonValue& v : d.array())
        if (v.isObject()) out.push_back(SupportTicket::fromJson(v.toObject()));
    return true;
}

bool SupportTicketStore::save(const QVector<SupportTicket>& tickets) const
{
    QDir().mkpath(dir_);
    QJsonArray arr;
    for (const SupportTicket& t : tickets) arr.append(t.toJson());
    // Write to a temp then atomically rename, so an interrupted write can't corrupt the store.
    const QString tmp = path_ + ".tmp";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const QByteArray bytes = QJsonDocument(arr).toJson(QJsonDocument::Indented);
    const bool ok = f.write(bytes) == bytes.size();
    f.flush();
    f.close();
    if (!ok) { QFile::remove(tmp); return false; }
    QFile::remove(path_);
    return QFile::rename(tmp, path_);
}

QString SupportTicketStore::add(SupportTicket t)
{
    QVector<SupportTicket> tickets;
    if (!load(tickets)) return {};

    const QByteArray hex = QUuid::createUuid().toRfc4122().left(4).toHex().toUpper();
    t.id        = QStringLiteral("TCK-") + QString::fromLatin1(hex);
    t.createdIso = QDateTime::currentDateTime().toString(Qt::ISODate);
    t.status    = Status::Received;

    tickets.push_back(t);
    if (!save(tickets)) return {};
    return t.id;
}

QVector<SupportTicket> SupportTicketStore::all() const
{
    QVector<SupportTicket> tickets;
    load(tickets);
    // Newest first (createdIso is sortable ISO; ties keep insertion order via stable sort).
    std::stable_sort(tickets.begin(), tickets.end(),
                     [](const SupportTicket& a, const SupportTicket& b){ return a.createdIso > b.createdIso; });
    return tickets;
}

bool SupportTicketStore::getById(const QString& id, SupportTicket& out) const
{
    QVector<SupportTicket> tickets;
    if (!load(tickets)) return false;
    for (const SupportTicket& t : tickets)
        if (t.id == id) { out = t; return true; }
    return false;
}

bool SupportTicketStore::mutate(const QString& id, const std::function<void(SupportTicket&)>& fn)
{
    QVector<SupportTicket> tickets;
    if (!load(tickets)) return false;
    bool found = false;
    for (SupportTicket& t : tickets)
        if (t.id == id) { fn(t); found = true; break; }
    if (!found) return false;
    return save(tickets);
}

bool SupportTicketStore::setStatus(const QString& id, Status s)
{
    return mutate(id, [s](SupportTicket& t){ t.status = s; });
}

bool SupportTicketStore::markValuable(const QString& id, const QString& rewardNote)
{
    return mutate(id, [&rewardNote](SupportTicket& t){ t.valuable = true; t.rewardNote = rewardNote; });
}

bool SupportTicketStore::setBundlePath(const QString& id, const QString& path)
{
    return mutate(id, [&path](SupportTicket& t){ t.bundlePath = path; });
}

} // namespace support
