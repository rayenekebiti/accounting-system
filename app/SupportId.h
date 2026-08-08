#ifndef APP_SUPPORT_ID_H
#define APP_SUPPORT_ID_H

#include <QString>

// Support identifier — a stable, per-install token a user can read aloud to support so their
// support bundles / crash reports can be correlated across time. It is:
//   • RANDOM — generated from a v4 UUID, NOT derived from hardware, MAC, disk, or any personal
//     data. It is therefore not a fingerprint and cannot identify a machine or person.
//   • LOCAL — persisted in the app's own settings; nothing is transmitted anywhere. This is NOT
//     telemetry: the app never phones home, and the ID only travels when the user *chooses* to
//     send a support bundle that contains it.
//   • STABLE — created once on first read and unchanged afterwards, so repeated reports from the
//     same install line up.
// Format: "OCC-XXXX-XXXX" (uppercase hex) — short enough to read over the phone.
namespace supportid {

QString get();   // returns the stable id, creating + persisting it on first call.

} // namespace supportid

#endif // APP_SUPPORT_ID_H
