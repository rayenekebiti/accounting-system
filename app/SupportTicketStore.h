#ifndef APP_SUPPORT_TICKET_STORE_H
#define APP_SUPPORT_TICKET_STORE_H

#include "SupportTicket.h"
#include <QString>
#include <QVector>
#include <functional>

// SupportTicketStore — a tiny local, file-backed store for SupportTickets (support metadata only,
// never accounting data). Persists a JSON array to <dir>/tickets.json. Single-user, single-threaded,
// desktop scale (a handful of tickets). No cloud, no sync, no transmission — a local record so the
// user (and, when they choose to share, support) can track a reported problem. Lives entirely above
// the accounting engine; creating/updating a ticket authors NO accounting events.
namespace support {

class SupportTicketStore {
public:
    explicit SupportTicketStore(QString dir);   // <dir>/tickets.json

    // Create a ticket: assigns a random local id + creation time + status=Received, persists, and
    // returns the assigned id (empty on write failure). `t.id`/`createdIso`/`status` are overwritten.
    QString add(SupportTicket t);

    QVector<SupportTicket> all() const;          // newest first
    bool getById(const QString& id, SupportTicket& out) const;

    bool setStatus(const QString& id, Status s);
    // Mark a ticket's feedback as valuable and attach a manual reward-eligibility note (the note is a
    // RECORD only — no discount is ever applied automatically).
    bool markValuable(const QString& id, const QString& rewardNote);
    bool setBundlePath(const QString& id, const QString& path);

    QString filePath() const { return path_; }

private:
    bool load(QVector<SupportTicket>& out) const;
    bool save(const QVector<SupportTicket>& tickets) const;
    bool mutate(const QString& id, const std::function<void(SupportTicket&)>& fn);

    QString dir_;
    QString path_;
};

} // namespace support

#endif // APP_SUPPORT_TICKET_STORE_H
