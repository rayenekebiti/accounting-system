#ifndef STORAGE_EVENT_LOG_H
#define STORAGE_EVENT_LOG_H
#include <cstddef>   // std::size_t

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// EventLog — append-only, crash-safe, CRC-framed record of authoritative history.
//
// This is the authoritative accounting history. Records are NEVER modified or
// deleted; corrections append new events. It is deliberately NOT BinaryRecordFile
// (which is fixed-size + random-access + soft-delete) — an audit log is variable-
// length, sequential, and immutable.
//
// On-disk format
// ──────────────
//   File header (32 bytes):
//     [ 0.. 7]  char[8]   magic = "ACCTLOG\0"
//     [ 8.. 9]  uint16_t  formatVersion (1)
//     [10..15]  reserved
//     [16..23]  uint64_t  committedLength  (authoritative end-of-log byte offset;
//                                            the commit point — see append())
//     [24..31]  reserved
//
//   Frame (one per event), starting at offset 32, packed back-to-back:
//     [ 0.. 3]  uint32_t  payloadLen
//     [ 4..11]  uint64_t  seq          (1-based, monotonic, gap-free)
//     [12..13]  uint16_t  type         (EventType)
//     [14..15]  uint16_t  schema       (per-event payload version)
//     [16..23]  int64_t   timestampMs  (wall clock; DISPLAY only — ordering is seq)
//     [24..27]  uint32_t  crc          (CRC-32 over bytes [0..23] + payload)
//     [28.....]  payload[payloadLen]
//   Frame size = 28 + payloadLen.
//
// Crash safety (the log is its own journal)
// ─────────────────────────────────────────
//   append(): ① write the frame at committedLength, fflush + fsync (durable);
//             ② advance header.committedLength, fflush + fsync (THE commit point).
//   A crash between ① and ② leaves a durable-but-uncommitted frame past
//   committedLength → on open it is truncated away (the event was never acked).
//   A crash mid-frame leaves a short/garbage tail → also truncated. The committed
//   region (≤ committedLength) is integrity-checked on open (CRC + gap-free seq).
//
// Ordering is by `seq`, assigned by the log — never by timestamp (clocks are not
// monotonic). Single-writer (guarded by the existing process QLockFile).
// ─────────────────────────────────────────────────────────────────────────────

struct EventRecord {
    uint64_t          seq         = 0;
    uint16_t          type        = 0;
    uint16_t          schema      = 0;
    int64_t           timestampMs = 0;
    std::vector<char> payload;
};

class EventLog {
public:
    explicit EventLog(std::string path);

    // Append one event. Assigns and returns the new seq (lastSeq + 1). Durable on
    // return (the event is committed history). Throws std::runtime_error on I/O fail.
    uint64_t append(uint16_t type, uint16_t schema, int64_t timestampMs,
                    const char* payload, uint32_t payloadLen);

    // One frame in an atomic group (see appendAtomic).
    struct FrameSpec {
        uint16_t          type        = 0;
        uint16_t          schema      = 0;
        int64_t           timestampMs = 0;
        std::vector<char> payload;
    };

    // Append a GROUP of events as ONE indivisible commit. All frames are written durably
    // PAST the commit point, then committedLength advances once past the whole group (the
    // single commit point). A crash before that advance leaves every frame durable-but-
    // uncommitted → all truncated on open (group absent); a crash after leaves them all
    // committed (group complete). There is no partial state. Returns the assigned seqs
    // (contiguous, gap-free). This is how one logical business fact spanning multiple
    // events (e.g. an invoice + its ledger posting) is authored atomically.
    std::vector<uint64_t> appendAtomic(const std::vector<FrameSpec>& frames);

    uint64_t    lastSeq()    const { return lastSeq_; }      // 0 = empty log
    std::size_t eventCount() const { return lastSeq_; }      // seq is 1-based + gap-free

    // Replay committed history in order. fn returns false to stop early.
    void forEach(const std::function<bool(const EventRecord&)>& fn);
    // Replay only events with seq > afterSeq (projection catch-up).
    void forEachAfter(uint64_t afterSeq, const std::function<bool(const EventRecord&)>& fn);

    // True if an uncommitted/torn tail was discarded when the log was opened.
    bool recoveredTornTail() const { return tornTail_; }

    static constexpr std::size_t kHeaderSize  = 32;
    static constexpr std::size_t kFrameHeader = 28;   // bytes before payload

private:
    std::string path_;
    uint64_t    committedLength_ = kHeaderSize;   // authoritative end of log
    uint64_t    lastSeq_         = 0;
    bool        tornTail_        = false;

    void openOrCreate();      // create header or recover + validate existing
    void writeFileHeader();   // (re)write the 32-byte header (fsync'd)
    void truncateTo(uint64_t length);
    void scanAndValidate();   // walk committed frames: CRC + gap-free seq; set lastSeq_
};

#endif // STORAGE_EVENT_LOG_H
