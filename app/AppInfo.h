#ifndef APP_APPINFO_H
#define APP_APPINFO_H

#include <QString>
#include <QStringList>

// Compile-time product identity + install-shape resolution for the commercial (C2) layer.
// This is the ONLY place version/channel/vendor constants live, so the installer, updater,
// crash reporter, support bundle, and About screen all agree. Nothing here touches the
// accounting engine.
namespace appinfo {

// ── Product identity (version stamping) ──────────────────────────────────────
inline constexpr const char* kProduct    = "Occountant";
inline constexpr const char* kVendor     = "RIO&JHK Technologies Co.";   // legal publisher (ARP / VersionInfo)
inline constexpr const char* kPublisher  = "RIO&JHK Technologies Co.";
inline constexpr const char* kOrg        = "Occountant";                 // QSettings / QStandardPaths org (path-clean)
inline constexpr const char* kCopyright  = "© RIO&JHK Technologies Co.";
inline constexpr const char* kSupportURL = "https://github.com/rayenekebiti/accounting-system";
// Stable install identity — the installer's AppId. NEVER change it: it is what lets a new
// installer recognise (and upgrade) a previous install rather than laying down a second copy.
inline constexpr const char* kInstallerAppId = "{{B6F4C9A2-8E71-4C33-9D2A-0F1E7A6B5C40}";

inline constexpr int         kVersionMajor = 1;
inline constexpr int         kVersionMinor = 0;
inline constexpr int         kVersionPatch = 0;
// Build id is stamped by CI/release.sh (-DACCT_BUILD_ID=...); falls back to "dev" for local builds.
#ifndef ACCT_BUILD_ID
#define ACCT_BUILD_ID "dev"
#endif
// Release channel this build was cut for (stable|rc|beta|development). The updater only offers a
// build whose channel is same-or-more-stable than the user's selected channel (see channelVisibleTo).
#ifndef ACCT_CHANNEL
#define ACCT_CHANNEL "stable"
#endif

inline QString version()   { return QString("%1.%2.%3").arg(kVersionMajor).arg(kVersionMinor).arg(kVersionPatch); }
inline QString buildId()   { return QStringLiteral(ACCT_BUILD_ID); }
inline QString channel()   { return QStringLiteral(ACCT_CHANNEL); }
inline QString fullVersion() { return version() + " (" + buildId() + ", " + channel() + ")"; }

// A monotonically-comparable integer for "is X newer than Y" (updater + installer downgrade gate).
inline long long versionCode(int maj, int min, int pat) { return (long long)maj*1000000 + min*1000 + pat; }
inline long long versionCode() { return versionCode(kVersionMajor, kVersionMinor, kVersionPatch); }

// The setup filename the release pipeline emits, e.g. "Occountant-1.0.0-Setup.exe".
inline QString installerBaseName(const QString& ver) { return QString("%1-%2-Setup.exe").arg(kProduct, ver); }
inline QString installerBaseName() { return installerBaseName(version()); }

// ── Release channels ─────────────────────────────────────────────────────────
// Ordered by INSTABILITY: Stable is the most conservative, Development the least. A user tracking
// a less-stable channel also receives everything from the more-stable channels (a Beta user sees
// Beta + RC + Stable builds), because a more-stable build is always a safe thing to offer.
enum class Channel { Stable = 0, RC = 1, Beta = 2, Development = 3 };

inline const char* channelName(Channel c)
{
    switch (c) {
        case Channel::Stable:      return "stable";
        case Channel::RC:          return "rc";
        case Channel::Beta:        return "beta";
        case Channel::Development: return "development";
    }
    return "stable";
}

inline QString channelDisplay(Channel c)
{
    switch (c) {
        case Channel::Stable:      return QStringLiteral("Stable");
        case Channel::RC:          return QStringLiteral("Release Candidate");
        case Channel::Beta:        return QStringLiteral("Beta");
        case Channel::Development: return QStringLiteral("Development");
    }
    return QStringLiteral("Stable");
}

// Case-insensitive parse; accepts "rc"/"releasecandidate", "dev"/"development". Unknown -> fallback.
inline Channel parseChannel(const QString& s, Channel fallback = Channel::Stable)
{
    const QString v = s.trimmed().toLower();
    if (v == "stable")                                   return Channel::Stable;
    if (v == "rc" || v == "releasecandidate")            return Channel::RC;
    if (v == "beta")                                     return Channel::Beta;
    if (v == "development" || v == "dev")                return Channel::Development;
    return fallback;
}

// The four channel ids, most-stable first (for a settings selector).
inline QStringList allChannels() { return { "stable", "rc", "beta", "development" }; }

// The channel THIS build was cut for.
inline Channel buildChannel() { return parseChannel(channel()); }

// Precedence rule: a build cut for `buildChan` is offered to a user tracking `userChan` iff the
// build is same-or-more-stable than what the user asked to track.
inline bool channelVisibleTo(Channel buildChan, Channel userChan)
{
    return static_cast<int>(buildChan) <= static_cast<int>(userChan);
}
inline bool channelVisibleTo(const QString& buildChan, const QString& userChan)
{
    return channelVisibleTo(parseChannel(buildChan), parseChannel(userChan));
}

// ── Downgrade gate (shared by the installer [Code] logic and its regression test) ──
// A setup whose version is strictly OLDER than what is already installed is a downgrade and must
// be refused: we never let an older build reopen books written by a newer one.
inline bool isDowngrade(long long installedCode, long long setupCode) { return setupCode < installedCode; }

} // namespace appinfo

#endif // APP_APPINFO_H
