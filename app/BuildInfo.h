#ifndef APP_BUILDINFO_H
#define APP_BUILDINFO_H

#include <QString>
#include <QByteArray>

// Deterministic, embedded build provenance. Every field is fixed at COMPILE time (from -D defines
// the release pipeline injects) or derived from Qt/compiler macros — never from a wall clock at
// run time — so BuildInfo for a given commit is reproducible and diff-stable. It is the single
// source of "exactly which build is this" for the About screen, crash reports, the support bundle,
// and the release manifest. Nothing here is on the accounting path.
namespace buildinfo {

struct Info {
    QString product;        // "Occountant"
    QString vendor;         // "RIO&JHK Technologies Co."
    QString version;        // "1.0.0"
    long long versionCode = 0;
    QString channel;        // "stable" | "rc" | "beta" | "development"
    QString buildId;        // release id ("dev" for local builds)
    QString gitCommit;      // short SHA, or "unknown"
    bool    gitDirty = false;
    QString buildTimestamp; // ISO-8601 UTC, DERIVED FROM THE COMMIT (git-authored), not wall clock
    long long buildEpoch = 0;
    QString compiler;       // "GCC 15.2.0", "Clang ...", "MSVC ..."
    QString qtVersion;      // QT_VERSION_STR at compile time
    QString platform;       // kernel type ("windows"/"darwin"/...)
    QString arch;           // build CPU architecture ("x86_64")
    QString abi;            // full build ABI string
};

const Info& info();

// Pretty, key-sorted JSON (QJsonObject sorts keys → byte-stable for equal inputs). This is the
// exact text embedded into BuildInfo.json in the install tree and the support bundle.
QByteArray json();

// One-line human summary for logs / the About footer.
QString oneLine();

} // namespace buildinfo

#endif // APP_BUILDINFO_H
