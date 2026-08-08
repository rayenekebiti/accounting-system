#include "BuildInfo.h"
#include "AppInfo.h"

#include <QJsonObject>
#include <QJsonDocument>
#include <QDateTime>
#include <QTimeZone>
#include <QSysInfo>

// Injected by CMake / tools/release.sh. Read WITHOUT redefining the macros (so passing them via
// -D never triggers a redefinition), and default a bare local build to honest, non-wall-clock
// values ("unknown" / 0) rather than silently stamping the current time — which would break
// reproducibility.
namespace {

#if defined(ACCT_GIT_COMMIT)
constexpr const char* kGitCommit = ACCT_GIT_COMMIT;
#else
constexpr const char* kGitCommit = "unknown";
#endif
#if defined(ACCT_GIT_DIRTY)
constexpr int kGitDirty = ACCT_GIT_DIRTY;
#else
constexpr int kGitDirty = 0;
#endif
#if defined(ACCT_BUILD_EPOCH)
constexpr long long kBuildEpoch = ACCT_BUILD_EPOCH;   // secs since epoch of the SOURCE commit, or 0
#else
constexpr long long kBuildEpoch = 0;
#endif

QString compilerString()
{
#if defined(__clang__)
    return QStringLiteral("Clang " __clang_version__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC " __VERSION__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#else
    return QStringLiteral("unknown");
#endif
}

buildinfo::Info build()
{
    buildinfo::Info i;
    i.product     = QString::fromLatin1(appinfo::kProduct);
    i.vendor      = QString::fromLatin1(appinfo::kVendor);
    i.version     = appinfo::version();
    i.versionCode = appinfo::versionCode();
    i.channel     = appinfo::channel();
    i.buildId     = appinfo::buildId();
    i.gitCommit   = QString::fromLatin1(kGitCommit);
    i.gitDirty    = (kGitDirty != 0);
    i.buildEpoch  = kBuildEpoch;
    i.buildTimestamp = i.buildEpoch > 0
        ? QDateTime::fromSecsSinceEpoch(i.buildEpoch, QTimeZone::UTC).toString(Qt::ISODate)
        : QStringLiteral("unknown");
    i.compiler    = compilerString();
    i.qtVersion   = QStringLiteral(QT_VERSION_STR);
    i.platform    = QSysInfo::kernelType();
    i.arch        = QSysInfo::buildCpuArchitecture();
    i.abi         = QSysInfo::buildAbi();
    return i;
}

} // namespace

namespace buildinfo {

const Info& info()
{
    static const Info kInfo = build();   // computed once; all inputs are compile-time constants
    return kInfo;
}

QByteArray json()
{
    const Info& i = info();
    QJsonObject o;
    o["product"]        = i.product;
    o["vendor"]         = i.vendor;
    o["version"]        = i.version;
    o["versionCode"]    = static_cast<double>(i.versionCode);
    o["channel"]        = i.channel;
    o["buildId"]        = i.buildId;
    o["gitCommit"]      = i.gitCommit;
    o["gitDirty"]       = i.gitDirty;
    o["buildTimestamp"] = i.buildTimestamp;
    o["buildEpoch"]     = static_cast<double>(i.buildEpoch);
    o["compiler"]       = i.compiler;
    o["qtVersion"]      = i.qtVersion;
    o["platform"]       = i.platform;
    o["arch"]           = i.arch;
    o["abi"]            = i.abi;
    // QJsonObject stores keys sorted → serialization is byte-stable for identical inputs.
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

QString oneLine()
{
    const Info& i = info();
    return QString("%1 %2 (%3, %4) commit %5%6 · %7 · Qt %8 · %9/%10")
        .arg(i.product, i.version, i.buildId, i.channel,
             i.gitCommit, i.gitDirty ? "+dirty" : "",
             i.compiler, i.qtVersion, i.platform, i.arch);
}

} // namespace buildinfo
