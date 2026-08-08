#ifndef BINARY_RECORD_FILE_H
#define BINARY_RECORD_FILE_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <fstream>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Layer 1 of the storage stack: type-erased fixed-size record I/O.
//
// v2 on-disk format
// ──────────────────
//   File header (32 bytes, always at offset 0):
//     [ 0.. 7]  char[8]   magic  = "ACCTPRO\0"
//     [ 8.. 9]  uint16_t  fileVersion   (container format; currently 2)
//     [10..11]  uint16_t  recordSize    (the record layout's size on disk)
//     [12..13]  uint16_t  schemaVersion (record LAYOUT version; 0 ≡ 1 for legacy files)
//     [14..15]  uint8_t   reserved[2]
//     [16..23]  uint64_t  lastWriteId   (monotonic; used for journal replay)
//     [24..31]  uint8_t   reserved[8]
//
// Schema versioning & forward migration
// ──────────────────────────────────────
//   `fileVersion` is the CONTAINER format (header + journal protocol) and changes
//   almost never. `schemaVersion` is the per-file RECORD LAYOUT and changes whenever
//   an entity gains/changes a field. The owner declares its current schemaVersion +
//   record size to the ctor; on open:
//     • file.schema  < code.schema  → migrate forward (atomically) before any read
//     • file.schema == code.schema  → sizes must agree, else a loud inconsistency
//     • file.schema  > code.schema  → REFUSE to open (old code must not corrupt a
//                                      newer file). Downgrade protection.
//   The default migration zero-extends each record to the new size — correct for the
//   APPEND-ONLY schema discipline (new fields appended, old offsets never move). A
//   custom MigrateFn handles the rare non-additive change.
//   Migration is crash-safe: write-ahead to a temp, durable fsync, atomic rename with
//   a backup, and recover-on-open — power loss leaves the file fully-old or fully-new.
//
//   Record data (immediately after the header):
//     record[id] starts at byte 32 + id * recordSize
//
//   Record count is derived from the file size; it is never stored in the
//   header to avoid partial-update corruption.
//
// Journal-based crash safety
// ───────────────────────────
//   Every append/update first writes a "journal" file (path + ".journal"):
//     [0..recordSize-1]  the new record bytes
//     [+0..+3]           uint32_t targetId  (position where the record goes)
//     [+4..+7]           uint32_t CRC-32 of the record bytes
//     [+8..+15]          uint64_t writeId   (> lastWriteId when written)
//
//   The journal is flushed to the OS before the main file is touched.
//   On open(), any leftover journal is replayed, then deleted.
//   This guarantees that a crash at any point in a write leaves the file
//   either unchanged or fully updated — never partially overwritten.
//
// ID space
// ─────────
//   IDs are uint32_t (≈ 4 billion records per file).
//   ID = position in the data area: record[id] at 32 + id * recordSize.
//
// Thread safety
// ─────────────
//   Single writer assumed. Never share an instance across threads.
// ─────────────────────────────────────────────────────────────────────────────
class BinaryRecordFile {
public:
    // Transforms one record from an older schema to the current layout. Receives the
    // on-disk schema being migrated FROM, the old record bytes + size, and the target
    // size; returns exactly `newSize` bytes. A null MigrateFn means "zero-extend"
    // (copy the old bytes, zero the appended tail) — correct for append-only changes.
    using MigrateFn = std::function<
        std::vector<char>(uint16_t fromSchema, const char* oldRecord,
                          std::size_t oldSize, std::size_t newSize)>;

    // deletedFlagOffset: byte within a record that is non-zero for soft-deleted
    // records. Used only by compact(); the class never inspects the bytes otherwise.
    // schemaVersion: the record layout this code expects (>= 1). migrate: optional
    // custom transform for non-additive evolution (default = zero-extend).
    BinaryRecordFile(std::string path, std::size_t recordSize, std::size_t deletedFlagOffset,
                     uint16_t schemaVersion = 1, MigrateFn migrate = {});

    // Appends a new record. Returns the assigned id (old count).
    // Throws std::runtime_error on I/O failure.
    uint32_t append(const char* buffer);

    // Reads record[id] into buffer. Returns false and zeroes buffer on failure.
    bool read(uint32_t id, char* buffer);

    // Overwrites record[id]. Returns false on range/I/O failure.
    bool update(uint32_t id, const char* buffer);

    // Number of records in the file (live + soft-deleted).
    std::size_t count();

    // Removes soft-deleted records, compacting the file in-place.
    // Returns a vector mapping old_id → new_id (UINT32_MAX if the record was dropped).
    // Caller is responsible for updating foreign keys.
    // progressFn is called with (done, total) as records are processed.
    std::vector<uint32_t> compact(
        std::function<void(std::size_t done, std::size_t total)> progressFn = nullptr);

    // True if a leftover journal was replayed when this file was opened (crash
    // recovery occurred). Read once after construction for startup diagnostics.
    bool recoveredOnOpen() const { return recovered_; }

    // True if a forward schema migration ran when this file was opened. For startup
    // diagnostics ("migrated invoices v1 → v2").
    bool migratedOnOpen() const { return migrated_; }

    // The record layout version currently on disk (after any migration on open).
    uint16_t schemaVersion() const { return schemaOnDisk_; }

    // Truncate to zero records (keep the header/schema). Used to rebuild a projection
    // from authoritative history — projections are disposable caches, not truth.
    void clear();

    // Deterministic content fingerprint: CRC-32 over the entire record region (in id
    // order, including soft-deleted tombstones — they are part of the deterministic
    // state). Two projections built from the same history hash identically; a bit-flip
    // or drift changes the hash. The container header is excluded (it is metadata).
    uint32_t contentHash();

    // CRC-32/ISO-HDLC (poly 0xEDB88320). Public so persistence tests can craft
    // valid/invalid journals deterministically.
    static uint32_t crc32(const char* data, std::size_t len);

private:
    std::fstream file_;
    std::string  path_;
    std::size_t  recordSize_;          // ACTIVE record size (file's, == target after migration)
    std::size_t  targetRecordSize_;    // the size this code expects for its schema
    std::size_t  deletedFlagOffset_;
    uint64_t     lastWriteId_ = 0;
    uint16_t     schemaTarget_ = 1;    // schema this code expects
    uint16_t     schemaOnDisk_ = 0;    // schema read from the file (0 ≡ 1 legacy)
    MigrateFn    migrate_;             // custom transform, or null → zero-extend

    static constexpr std::size_t kHeaderSize = 32;
    static constexpr uint16_t    kVersion    = 2;

    // ── Header helpers ──────────────────────────────────────────────────────
    void         writeHeader();
    void         readHeader();     // fills lastWriteId_/schemaOnDisk_/recordSize_ (no throw on size)

    // ── Schema migration ────────────────────────────────────────────────────
    std::string  migratingPath() const { return path_ + ".migrating"; }
    std::string  migrateBakPath() const { return path_ + ".migrate.bak"; }
    void         recoverInterruptedMigration();  // finish/rollback a crashed migration (run first)
    void         maybeMigrateSchema();           // migrate forward / refuse downgrade
    void         performSchemaMigration(uint16_t fromSchema, std::size_t oldSize, std::size_t newSize);

    // ── Journal helpers ─────────────────────────────────────────────────────
    std::string  journalPath()  const { return path_ + ".journal"; }
    void         writeJournal(const char* record, uint32_t targetId, uint64_t writeId);
    void         replayJournal();   // no-op if no journal exists
    void         deleteJournal();

    // Migrate a headerless (v0/v1) file by prepending the 32-byte header.
    void migrateFromHeaderless();

    // Force the main file's record + header to stable storage (fsync). Closes the
    // power-loss window: the journal is only deleted once the data is durable.
    // Best-effort — see the .cpp for the degraded path. Returns true if it synced.
    bool syncToDisk();

    bool recovered_ = false;   // set by replayJournal() when it applies a journal
    bool migrated_  = false;   // set by performSchemaMigration() when it runs

    bool openFile();   // (re)open file_, returns true on success

    // Data area offset for a given id.
    std::streamoff dataOffset(uint32_t id) const
    {
        return static_cast<std::streamoff>(kHeaderSize)
             + static_cast<std::streamoff>(id) * static_cast<std::streamoff>(recordSize_);
    }
};

#endif // BINARY_RECORD_FILE_H
