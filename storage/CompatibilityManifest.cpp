#include "CompatibilityManifest.h"
#include "BinaryRecordFile.h"   // crc32
#include "FaultInjection.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#  include <io.h>
#  define CM_FSYNC(fd) _commit(fd)
#else
#  include <unistd.h>
#  define CM_FSYNC(fd) ::fsync(fd)
#endif

namespace {

// Same crash-injection discipline as AuditJournal::ajMaybeCrash — hard-exit at a named
// point so the cross-process crash test can prove complete-or-absent write ordering.
inline void cmMaybeCrash(const char* point)
{
    const char* want = std::getenv("ACCT_CRASH_POINT");
    if (want && std::strcmp(want, point) == 0) { std::fflush(nullptr); std::_Exit(99); }
}

constexpr char kMagic[8] = { 'A','C','C','T','M','F','S','T' };
constexpr uint16_t kFileFormat = 1;

void putU16(char* p, uint16_t v) { std::memcpy(p, &v, 2); }
uint16_t getU16(const char* p) { uint16_t v = 0; std::memcpy(&v, p, 2); return v; }

} // namespace

namespace compat {

GovernanceVersions current()
{
    return GovernanceVersions{
        kSchemaVersion, kReplayVersion, kPostingPolicyVersion, kStatementVersion,
        kSnapshotVersion, kEventLogFormatVersion, kEngineBuild };
}

GovernanceVersions floors()
{
    return GovernanceVersions{
        kMinSchema, kMinReplay, kMinPostingPolicy, kMinStatement,
        kMinSnapshot, kMinEventLogFormat, /*engineBuild floor*/ 0 };
}

const char* toString(Compatibility c)
{
    switch (c) {
    case Compatibility::Compatible:        return "compatible";
    case Compatibility::MigrationRequired: return "migration-required";
    case Compatibility::Incompatible:      return "incompatible";
    }
    return "unknown";
}

Compatibility classify(const GovernanceVersions& onDisk,
                       const GovernanceVersions& code,
                       const GovernanceVersions& fl,
                       std::string& reason)
{
    // Compare each axis. engineBuild is informational and never gates compatibility.
    struct Axis { const char* name; uint16_t disk, code, floor; };
    const Axis axes[] = {
        { "schema",         onDisk.schema,         code.schema,         fl.schema },
        { "replay",         onDisk.replay,         code.replay,         fl.replay },
        { "postingPolicy",  onDisk.postingPolicy,  code.postingPolicy,  fl.postingPolicy },
        { "statement",      onDisk.statement,      code.statement,      fl.statement },
        { "snapshot",       onDisk.snapshot,       code.snapshot,       fl.snapshot },
        { "eventLogFormat", onDisk.eventLogFormat, code.eventLogFormat, fl.eventLogFormat },
    };

    bool migrationRequired = false;
    for (const Axis& a : axes) {
        if (a.disk > a.code) {
            reason = std::string("data '") + a.name + "' version " + std::to_string(a.disk)
                   + " is newer than this build's " + std::to_string(a.code)
                   + " — these books were written by a newer AccountingPro; upgrade to open them";
            return Compatibility::Incompatible;   // downgrade protection
        }
        // 0 == unset/pre-governance → read as baseline, not a downgrade.
        if (a.disk != 0 && a.disk < a.floor) {
            reason = std::string("data '") + a.name + "' version " + std::to_string(a.disk)
                   + " is older than the minimum this build can migrate from ("
                   + std::to_string(a.floor) + ") — reinterpreting it is unsafe";
            return Compatibility::Incompatible;   // too old to reinterpret
        }
        if (a.disk != 0 && a.disk < a.code)
            migrationRequired = true;
    }

    if (migrationRequired) {
        reason = "an older but supported data version requires forward migration";
        return Compatibility::MigrationRequired;
    }
    reason.clear();
    return Compatibility::Compatible;
}

Compatibility classify(const GovernanceVersions& onDisk, std::string& reason)
{
    return classify(onDisk, current(), floors(), reason);
}

} // namespace compat

bool CompatibilityManifest::read(const std::string& path, GovernanceVersions& out)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[kSize];
    const bool full = std::fread(buf, 1, kSize, f) == kSize;
    std::fclose(f);
    if (!full) return false;
    if (std::memcmp(buf, kMagic, 8) != 0) return false;
    if (getU16(buf + 8) != kFileFormat) return false;   // unknown manifest format → rebuild

    uint32_t stored = 0; std::memcpy(&stored, buf + 32, 4);
    if (BinaryRecordFile::crc32(buf, 32) != stored) return false;   // corrupt → rebuild

    out.schema         = getU16(buf + 10);
    out.replay         = getU16(buf + 12);
    out.postingPolicy  = getU16(buf + 14);
    out.statement      = getU16(buf + 16);
    out.snapshot       = getU16(buf + 18);
    out.eventLogFormat = getU16(buf + 20);
    out.engineBuild    = getU16(buf + 22);
    return true;
}

void CompatibilityManifest::write(const std::string& path, const GovernanceVersions& v)
{
    char buf[kSize];
    std::memset(buf, 0, kSize);
    std::memcpy(buf, kMagic, 8);
    putU16(buf + 8,  kFileFormat);
    putU16(buf + 10, v.schema);
    putU16(buf + 12, v.replay);
    putU16(buf + 14, v.postingPolicy);
    putU16(buf + 16, v.statement);
    putU16(buf + 18, v.snapshot);
    putU16(buf + 20, v.eventLogFormat);
    putU16(buf + 22, v.engineBuild);
    const uint32_t crc = BinaryRecordFile::crc32(buf, 32);
    std::memcpy(buf + 32, &crc, 4);

    // Fault injection: the manifest write fails. The manifest is a disposable projection of
    // the EngineVersionStamp events, so StorageService ignores the fault and the next open
    // rebuilds it from the log. Proves the manifest never becomes a single point of failure.
    if (acctFaultAt("manifestWrite"))
        throw std::runtime_error("CompatibilityManifest: injected fault at manifestWrite for " + path);

    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) throw std::runtime_error("CompatibilityManifest: cannot create temp " + tmp);
    const bool ok = std::fwrite(buf, 1, kSize, f) == kSize;
    if (ok) std::fflush(f);
    if (ok) CM_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) {
        std::error_code ec; std::filesystem::remove(tmp, ec);
        throw std::runtime_error("CompatibilityManifest: write failed " + tmp);
    }

    cmMaybeCrash("afterManifestTmp");   // temp durable, not yet installed → old/no manifest stands
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
    if (ec) throw std::runtime_error("CompatibilityManifest: install failed " + path
                                     + " (" + ec.message() + ")");
}
