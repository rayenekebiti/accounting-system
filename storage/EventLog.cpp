#include "EventLog.h"
#include "BinaryRecordFile.h"   // crc32
#include "FaultInjection.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#ifdef _WIN32
#  include <io.h>
#  define EVTLOG_FSYNC(fd)     _commit(fd)
#  define EVTLOG_SEEK(f, off)  _fseeki64((f), static_cast<long long>(off), SEEK_SET)
#else
#  include <unistd.h>
#  define EVTLOG_FSYNC(fd)     ::fsync(fd)
#  define EVTLOG_SEEK(f, off)  fseeko((f), static_cast<off_t>(off), SEEK_SET)
#endif

// Crash-injection hook (shared env contract with the storage layer). When
// ACCT_CRASH_POINT names a step, the process hard-exits there — used by the
// deterministic crash-window tests.
static inline void evtMaybeCrash(const char* point)
{
    const char* want = std::getenv("ACCT_CRASH_POINT");
    if (want && std::strcmp(want, point) == 0) { std::fflush(nullptr); std::_Exit(99); }
}

static constexpr char kMagic[8] = {'A','C','C','T','L','O','G','\0'};

// Build one framed event (kFrameHeader + payload) with its CRC, ready to write at the
// tail. Shared by the atomic-group path; the single-frame append() keeps its own copy.
static std::vector<char> buildFrame(uint64_t seq, uint16_t type, uint16_t schema,
                                    int64_t timestampMs, const char* payload, uint32_t payloadLen)
{
    std::vector<char> frame(EventLog::kFrameHeader + payloadLen);
    std::memcpy(frame.data() + 0,  &payloadLen,  4);
    std::memcpy(frame.data() + 4,  &seq,         8);
    std::memcpy(frame.data() + 12, &type,        2);
    std::memcpy(frame.data() + 14, &schema,      2);
    std::memcpy(frame.data() + 16, &timestampMs, 8);
    std::vector<char> crcbuf(24 + payloadLen);
    std::memcpy(crcbuf.data(), frame.data(), 24);
    if (payloadLen) std::memcpy(crcbuf.data() + 24, payload, payloadLen);
    const uint32_t crc = BinaryRecordFile::crc32(crcbuf.data(), crcbuf.size());
    std::memcpy(frame.data() + 24, &crc, 4);
    if (payloadLen) std::memcpy(frame.data() + EventLog::kFrameHeader, payload, payloadLen);
    return frame;
}

EventLog::EventLog(std::string path) : path_(std::move(path))
{
    openOrCreate();
}

void EventLog::writeFileHeader()
{
    // Fault injection: the commit-point header write fails. The in-memory committedLength_
    // is ahead, but on disk it still points before the just-written frame(s) → on the next
    // open the uncommitted tail is truncated (the event/group is cleanly ABSENT). Proves
    // atomicity survives a failed commit-point write.
    if (acctFaultAt("logCommit"))
        throw std::runtime_error("EventLog: injected fault at logCommit (header write) in " + path_);

    char buf[kHeaderSize] = {};
    std::memcpy(buf, kMagic, 8);
    const uint16_t ver = 1;
    std::memcpy(buf + 8, &ver, 2);
    std::memcpy(buf + 16, &committedLength_, 8);

    FILE* f = std::fopen(path_.c_str(), "r+b");
    if (!f) throw std::runtime_error("EventLog: cannot open for header write: " + path_);
    std::fseek(f, 0, SEEK_SET);
    const bool ok = std::fwrite(buf, 1, kHeaderSize, f) == kHeaderSize;
    if (ok) std::fflush(f);
    if (ok) EVTLOG_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) throw std::runtime_error("EventLog: header write failed: " + path_);
}

void EventLog::truncateTo(uint64_t length)
{
    std::error_code ec;
    std::filesystem::resize_file(path_, length, ec);
}

void EventLog::openOrCreate()
{
    namespace fs = std::filesystem;
    std::error_code ec;

    if (!fs::exists(path_, ec) || fs::file_size(path_, ec) == 0) {
        committedLength_ = kHeaderSize;
        lastSeq_         = 0;
        FILE* f = std::fopen(path_.c_str(), "wb");   // create
        if (!f) throw std::runtime_error("EventLog: cannot create " + path_);
        std::fclose(f);
        writeFileHeader();
        return;
    }

    // Read the file header.
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) throw std::runtime_error("EventLog: cannot open " + path_);
    char buf[kHeaderSize] = {};
    const std::size_t got = std::fread(buf, 1, kHeaderSize, f);
    std::fclose(f);
    if (got != kHeaderSize || std::memcmp(buf, kMagic, 8) != 0)
        throw std::runtime_error("EventLog: missing/short header in " + path_);
    uint16_t ver = 0;
    std::memcpy(&ver, buf + 8, 2);
    if (ver != 1)
        throw std::runtime_error("EventLog: unsupported format version "
                                 + std::to_string(ver) + " in " + path_);
    std::memcpy(&committedLength_, buf + 16, 8);
    if (committedLength_ < kHeaderSize) committedLength_ = kHeaderSize;

    const auto actual = static_cast<uint64_t>(fs::file_size(path_, ec));
    if (actual > committedLength_) {
        // Discard a durable-but-uncommitted (or torn) tail past the commit point.
        truncateTo(committedLength_);
        tornTail_ = true;
    } else if (committedLength_ > actual) {
        // The header claims MORE committed bytes than the file physically holds — corruption
        // (a legit commit point never exceeds the file, since frames are written before it
        // advances). Trust the file, not the field: clamp so scanAndValidate bounds every
        // frame length to real bytes (no oversized allocation from a crafted length) and then
        // rejects the resulting torn tail LOUDLY rather than OOM-ing on a huge `payloadLen`.
        committedLength_ = actual;
        tornTail_ = true;
    }

    scanAndValidate();
}

void EventLog::scanAndValidate()
{
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) throw std::runtime_error("EventLog: cannot open for scan: " + path_);

    uint64_t off       = kHeaderSize;
    uint64_t expectSeq = 1;
    bool     ok        = true;

    while (off + kFrameHeader <= committedLength_) {
        EVTLOG_SEEK(f, off);
        char hdr[kFrameHeader];
        if (std::fread(hdr, 1, kFrameHeader, f) != kFrameHeader) { ok = false; break; }

        uint32_t len = 0;       std::memcpy(&len, hdr + 0, 4);
        uint64_t seq = 0;       std::memcpy(&seq, hdr + 4, 8);
        uint32_t storedCrc = 0; std::memcpy(&storedCrc, hdr + 24, 4);

        const uint64_t frameEnd = off + kFrameHeader + len;
        if (frameEnd > committedLength_) { ok = false; break; }   // claims more than committed

        std::vector<char> crcbuf(24 + len);
        std::memcpy(crcbuf.data(), hdr, 24);
        if (len && std::fread(crcbuf.data() + 24, 1, len, f) != len) { ok = false; break; }

        if (BinaryRecordFile::crc32(crcbuf.data(), crcbuf.size()) != storedCrc) { ok = false; break; }
        if (seq != expectSeq) { ok = false; break; }              // gap / reorder

        ++expectSeq;
        off = frameEnd;
    }
    std::fclose(f);

    if (!ok || off != committedLength_)
        throw std::runtime_error(
            "EventLog: corrupt committed history in " + path_
            + " (failed near offset " + std::to_string(off) + ")");

    lastSeq_ = expectSeq - 1;
}

uint64_t EventLog::append(uint16_t type, uint16_t schema, int64_t timestampMs,
                          const char* payload, uint32_t payloadLen)
{
    const uint64_t seq = lastSeq_ + 1;

    std::vector<char> frame(kFrameHeader + payloadLen);
    std::memcpy(frame.data() + 0,  &payloadLen,  4);
    std::memcpy(frame.data() + 4,  &seq,         8);
    std::memcpy(frame.data() + 12, &type,        2);
    std::memcpy(frame.data() + 14, &schema,      2);
    std::memcpy(frame.data() + 16, &timestampMs, 8);

    std::vector<char> crcbuf(24 + payloadLen);
    std::memcpy(crcbuf.data(), frame.data(), 24);
    if (payloadLen) std::memcpy(crcbuf.data() + 24, payload, payloadLen);
    const uint32_t crc = BinaryRecordFile::crc32(crcbuf.data(), crcbuf.size());
    std::memcpy(frame.data() + 24, &crc, 4);
    if (payloadLen) std::memcpy(frame.data() + kFrameHeader, payload, payloadLen);

    // ① Write the frame past the commit point; make it durable.
    FILE* f = std::fopen(path_.c_str(), "r+b");
    if (!f) throw std::runtime_error("EventLog: cannot open for append: " + path_);
    EVTLOG_SEEK(f, committedLength_);
    const bool ok = std::fwrite(frame.data(), 1, frame.size(), f) == frame.size();
    if (ok) std::fflush(f);
    if (ok) EVTLOG_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) throw std::runtime_error("EventLog: frame write failed in " + path_);

    evtMaybeCrash("afterEventFrame");   // frame durable, NOT yet committed

    // ② Advance the committed length — the commit point.
    committedLength_ += frame.size();
    writeFileHeader();
    lastSeq_ = seq;

    evtMaybeCrash("afterEventCommit");  // fully committed history
    return seq;
}

std::vector<uint64_t> EventLog::appendAtomic(const std::vector<FrameSpec>& frames)
{
    std::vector<uint64_t> seqs;
    if (frames.empty()) return seqs;

    // Build every frame + assign contiguous, gap-free seqs.
    std::vector<std::vector<char>> built;
    built.reserve(frames.size());
    uint64_t seq = lastSeq_;
    uint64_t totalBytes = 0;
    for (const FrameSpec& fs : frames) {
        ++seq;
        seqs.push_back(seq);
        built.push_back(buildFrame(seq, fs.type, fs.schema, fs.timestampMs,
                                   fs.payload.data(), static_cast<uint32_t>(fs.payload.size())));
        totalBytes += built.back().size();
    }

    // ① Write EVERY frame past the commit point and make them durable. committedLength is
    //    NOT advanced yet, so until ② the whole group is a discardable uncommitted tail —
    //    a crash here truncates all of it on open (the group is ABSENT, never partial).
    FILE* f = std::fopen(path_.c_str(), "r+b");
    if (!f) throw std::runtime_error("EventLog: cannot open for atomic append: " + path_);
    EVTLOG_SEEK(f, committedLength_);
    bool ok = true;
    for (std::size_t i = 0; i < built.size() && ok; ++i) {
        ok = std::fwrite(built[i].data(), 1, built[i].size(), f) == built[i].size();
        if (ok && i == 0) {   // make the first frame durable, then offer the injection point
            std::fflush(f);
            EVTLOG_FSYNC(fileno(f));
            evtMaybeCrash("afterTxnFirstFrame");   // first frame durable, group NOT committed → absent
        }
    }
    if (ok) std::fflush(f);
    if (ok) EVTLOG_FSYNC(fileno(f));
    std::fclose(f);
    if (!ok) throw std::runtime_error("EventLog: atomic frame write failed in " + path_);

    // ② Advance the committed length past the WHOLE group — the single commit point.
    committedLength_ += totalBytes;
    writeFileHeader();
    lastSeq_ = seq;

    evtMaybeCrash("afterTxnCommit");   // fully committed group (all frames present)
    return seqs;
}

void EventLog::forEachAfter(uint64_t afterSeq, const std::function<bool(const EventRecord&)>& fn)
{
    FILE* f = std::fopen(path_.c_str(), "rb");
    if (!f) return;

    uint64_t off = kHeaderSize;
    while (off + kFrameHeader <= committedLength_) {
        EVTLOG_SEEK(f, off);
        char hdr[kFrameHeader];
        if (std::fread(hdr, 1, kFrameHeader, f) != kFrameHeader) break;

        uint32_t len = 0; std::memcpy(&len, hdr + 0, 4);
        EventRecord r;
        std::memcpy(&r.seq,         hdr + 4,  8);
        std::memcpy(&r.type,        hdr + 12, 2);
        std::memcpy(&r.schema,      hdr + 14, 2);
        std::memcpy(&r.timestampMs, hdr + 16, 8);
        r.payload.resize(len);
        if (len && std::fread(r.payload.data(), 1, len, f) != len) break;

        off += kFrameHeader + len;
        if (r.seq > afterSeq && !fn(r)) break;
    }
    std::fclose(f);
}

void EventLog::forEach(const std::function<bool(const EventRecord&)>& fn)
{
    forEachAfter(0, fn);
}
