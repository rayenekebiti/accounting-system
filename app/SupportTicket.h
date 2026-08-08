#ifndef APP_SUPPORT_TICKET_H
#define APP_SUPPORT_TICKET_H

#include <QString>
#include <QJsonObject>

// SupportTicket — a locally-stored record of a problem report / feedback item. This is SUPPORT
// metadata, deliberately kept ABOVE and SEPARATE from the accounting engine: it never enters the
// EventLog, is never an accounting fact, and is stored in its own file. It carries no customer /
// invoice / payment / company data — only what the user typed into the report form plus a category,
// severity, and lifecycle status. Nothing here is transmitted; the user chooses whether to send.
namespace support {

// Lifecycle. Operator-driven; the app only creates tickets as "Received".
//   Received → Reviewing → Confirmed → Fixed → Released
enum class Status { Received, Reviewing, Confirmed, Fixed, Released };

inline const char* statusKey(Status s)
{
    switch (s) {
        case Status::Received:  return "Received";
        case Status::Reviewing: return "Reviewing";
        case Status::Confirmed: return "Confirmed";
        case Status::Fixed:     return "Fixed";
        case Status::Released:  return "Released";
    }
    return "Received";
}

inline Status statusFromKey(const QString& k)
{
    if (k == QLatin1String("Reviewing")) return Status::Reviewing;
    if (k == QLatin1String("Confirmed")) return Status::Confirmed;
    if (k == QLatin1String("Fixed"))     return Status::Fixed;
    if (k == QLatin1String("Released"))  return Status::Released;
    return Status::Received;
}

struct SupportTicket {
    QString id;             // "TCK-XXXXXXXX" — local, random, not identifying
    QString category;       // Accounting | Invoice | ... | Other (stored as the untranslated key)
    QString severity;       // Blocking | Important | Minor | Suggestion (untranslated key)
    QString whatHappened;   // free text (the user's words)
    QString expected;       // free text
    QString steps;          // free text
    QString createdIso;     // creation timestamp (local ISO)
    Status  status = Status::Received;
    bool    valuable = false;   // operator marks feedback as valuable (reward eligibility)
    QString rewardNote;         // generated eligibility record (manual grant — never auto-applied)
    QString supportId;          // the install's support id at creation (for correlation)
    QString bundlePath;         // path to an attached diagnostics bundle, if generated

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["id"] = id;                 o["category"] = category;   o["severity"] = severity;
        o["whatHappened"] = whatHappened; o["expected"] = expected; o["steps"] = steps;
        o["createdIso"] = createdIso; o["status"] = QString::fromLatin1(statusKey(status));
        o["valuable"] = valuable;     o["rewardNote"] = rewardNote;
        o["supportId"] = supportId;   o["bundlePath"] = bundlePath;
        return o;
    }

    static SupportTicket fromJson(const QJsonObject& o)
    {
        SupportTicket t;
        t.id = o.value("id").toString();             t.category = o.value("category").toString();
        t.severity = o.value("severity").toString(); t.whatHappened = o.value("whatHappened").toString();
        t.expected = o.value("expected").toString(); t.steps = o.value("steps").toString();
        t.createdIso = o.value("createdIso").toString();
        t.status = statusFromKey(o.value("status").toString());
        t.valuable = o.value("valuable").toBool();   t.rewardNote = o.value("rewardNote").toString();
        t.supportId = o.value("supportId").toString(); t.bundlePath = o.value("bundlePath").toString();
        return t;
    }
};

} // namespace support

#endif // APP_SUPPORT_TICKET_H
