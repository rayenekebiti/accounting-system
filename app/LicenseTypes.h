#ifndef APP_LICENSE_TYPES_H
#define APP_LICENSE_TYPES_H

#include <QString>
#include <QStringList>
#include <cstdint>

// Read-only license vocabulary shared by the manager, the UI view-model, and the tests. The
// accounting engine never sees any of this — licensing lives entirely above StorageService.
namespace lic {

// Lifecycle state the app reacts to.
enum class State {
    Trial,      // valid, time-limited evaluation
    Personal,   // valid paid — Personal edition
    Business,   // valid paid — Business edition
    Expired,    // was valid; past expiry (+ grace)
    Invalid     // missing / tampered / unparseable / unsigned
};

// The edition a license grants (independent of whether it is currently valid).
enum class Edition { Trial, Personal, Business };

struct Status {
    State    state          = State::Invalid;
    Edition  edition        = Edition::Trial;
    QString  issuedTo;
    QString  licenseId;
    qint64   issuedAtEpoch  = 0;
    qint64   expiresAtEpoch = 0;   // 0 = perpetual
    qint64   daysRemaining  = 0;   // to expiry (0 for perpetual / already expired)
    bool     inGrace        = false;
    bool     valid          = false;  // usable: state is Trial/Personal/Business
    QString  detail;                  // human-readable note (why invalid / in grace)
    QStringList features;             // vendor-enabled feature keys (empty for the self-issued trial)
};

inline const char* stateName(State s)
{
    switch (s) {
    case State::Trial:    return "Trial";
    case State::Personal: return "Personal";
    case State::Business: return "Business";
    case State::Expired:  return "Expired";
    case State::Invalid:  return "Invalid";
    }
    return "Invalid";
}

inline const char* editionName(Edition e)
{
    switch (e) {
    case Edition::Trial:    return "Trial";
    case Edition::Personal: return "Personal";
    case Edition::Business: return "Business";
    }
    return "Trial";
}

} // namespace lic

#endif // APP_LICENSE_TYPES_H
