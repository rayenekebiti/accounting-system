#include "BinaryRecordFile.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Crash-injection hook for deterministic recovery testing. When env
// ACCT_CRASH_POINT == <point>, the process hard-exits (no fstream flush, no
// destructors) — simulating an app kill at that exact step. The OS page cache
// survives a process exit, so flushed writes persist but un-flushed ones are lost.
static inline void acctMaybeCrash(const char* point)
{
    const char* want = std::getenv("ACCT_CRASH_POINT");
    if (want && std::strcmp(want, point) == 0) {
        std::fflush(nullptr);
        std::_Exit(99);
    }
}

#ifdef _WIN32
#  include <io.h>      // _commit, _fileno
#  define PLATFORM_FSYNC(fd) _commit(fd)
#else
#  include <unistd.h>
#  define PLATFORM_FSYNC(fd) ::fsync(fd)
#endif

// ── CRC-32/ISO-HDLC ─────────────────────────────────────────────────────────
uint32_t BinaryRecordFile::crc32(const char* data, std::size_t len)
{
    static const auto table = []() {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();

    uint32_t crc = 0xFFFFFFFFUL;
    for (std::size_t i = 0; i < len; ++i)
        crc = table[static_cast<uint8_t>(crc ^ data[i])] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFUL;
}

// ── File open / create ───────────────────────────────────────────────────────
bool BinaryRecordFile::openFile()
{
    if (file_.is_open()) return true;
    file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    if (!file_.is_open()) {
        file_.clear();
        // Create the file if it does not exist.
        file_.open(path_, std::ios::out | std::ios::binary);
        file_.close();
        file_.open(path_, std::ios::in | std::ios::out | std::ios::binary);
    }
    return file_.is_open();
}

BinaryRecordFile::BinaryRecordFile(std::string path,
                                   std::size_t recordSize,
                                   std::size_t deletedFlagOffset,
                                   uint16_t    schemaVersion,
                                   MigrateFn   migrate)
    : path_(std::move(path))
    , recordSize_(recordSize)
    , targetRecordSize_(recordSize)
    , deletedFlagOffset_(deletedFlagOffset)
    , schemaTarget_(schemaVersion ? schemaVersion : uint16_t{1})
    , migrate_(std::move(migrate))
{
    // Finish or roll back a migration interrupted by a previous crash BEFORE we
    // interpret the file — a half-migrated state (original renamed aside, new file
    // not yet in place) must not be mistaken for a fresh/empty file and discarded.
    recoverInterruptedMigration();

    if (!openFile())
        throw std::runtime_error("BinaryRecordFile: failed to open " + path_);

    // Determine whether the file already has a v2 header.
    file_.seekg(0, std::ios::end);
    const auto fileSize = static_cast<std::size_t>(file_.tellg());

    if (fileSize == 0) {
        // Brand-new file: stamp the current schema + record size.
        schemaOnDisk_ = schemaTarget_;
        recordSize_   = targetRecordSize_;
        writeHeader();
    } else {
        // Peek at the magic bytes.
        char magic[8] = {};
        file_.seekg(0, std::ios::beg);
        file_.read(magic, 8);

        static constexpr char kMagic[8] = {'A','C','C','T','P','R','O','\0'};
        if (std::memcmp(magic, kMagic, 8) == 0) {
            // Header present — load lastWriteId + the on-disk schema/record size.
            readHeader();
        } else {
            // Old headerless file — prepend a header, then stamp the current schema.
            migrateFromHeaderless();
            schemaOnDisk_ = schemaTarget_;
            recordSize_   = targetRecordSize_;
            writeHeader();
        }
    }

    // Replay any leftover journal at the file's CURRENT (pre-migration) record size —
    // journal entries are sized to the on-disk layout.
    replayJournal();

    // Bring the layout up to the code's schema (atomic + crash-safe). No-op when the
    // file is already current; throws when the file is NEWER than this code.
    maybeMigrateSchema();
}

// ── Header write/read ────────────────────────────────────────────────────────
void BinaryRecordFile::writeHeader()
{
    char buf[kHeaderSize] = {};
    static constexpr char kMagic[8] = {'A','C','C','T','P','R','O','\0'};
    std::memcpy(buf + 0, kMagic, 8);

    const uint16_t ver = kVersion;
    std::memcpy(buf + 8, &ver, 2);

    const uint16_t rs = static_cast<uint16_t>(recordSize_);
    std::memcpy(buf + 10, &rs, 2);

    const uint16_t schema = schemaOnDisk_ ? schemaOnDisk_ : schemaTarget_;
    std::memcpy(buf + 12, &schema, 2);

    std::memcpy(buf + 16, &lastWriteId_, 8);

    file_.seekp(0, std::ios::beg);
    file_.write(buf, kHeaderSize);
    file_.flush();
}

void BinaryRecordFile::readHeader()
{
    char buf[kHeaderSize] = {};
    file_.seekg(0, std::ios::beg);
    file_.read(buf, kHeaderSize);
    if (file_.gcount() != kHeaderSize) {
        file_.clear();
        throw std::runtime_error("BinaryRecordFile: header read failed in " + path_);
    }

    uint16_t ver = 0;
    std::memcpy(&ver, buf + 8, 2);
    if (ver != kVersion)
        throw std::runtime_error(
            "BinaryRecordFile: file version " + std::to_string(ver)
            + " not supported (expected " + std::to_string(kVersion) + ") in " + path_);

    // The on-disk record size drives our ACTIVE size until migration aligns it to the
    // code's target — we no longer throw on a mismatch; that is now a migration input.
    uint16_t rs = 0;
    std::memcpy(&rs, buf + 10, 2);
    if (rs == 0)
        throw std::runtime_error("BinaryRecordFile: corrupt header (record size 0) in " + path_);
    recordSize_ = static_cast<std::size_t>(rs);

    // schemaVersion: 0 in legacy v2 files means "baseline" → schema 1.
    uint16_t schema = 0;
    std::memcpy(&schema, buf + 12, 2);
    schemaOnDisk_ = schema ? schema : uint16_t{1};

    std::memcpy(&lastWriteId_, buf + 16, 8);
}

// ── Headerless migration ─────────────────────────────────────────────────────
// Called when a v0/v1 file has no header. We slide all records forward by
// kHeaderSize bytes and then write the header. Uses a temp file to stay safe.
void BinaryRecordFile::migrateFromHeaderless()
{
    file_.seekg(0, std::ios::end);
    const auto oldSize = static_cast<std::size_t>(file_.tellg());

    // Read all records into memory.
    std::vector<char> data(oldSize);
    file_.seekg(0, std::ios::beg);
    file_.read(data.data(), static_cast<std::streamsize>(oldSize));
    if (static_cast<std::size_t>(file_.gcount()) != oldSize) {
        file_.clear();
        throw std::runtime_error("BinaryRecordFile: migration read failed in " + path_);
    }

    // Write to a temp file (header + original bytes).
    const std::string tmpPath = path_ + ".migrating";
    {
        std::fstream tmp(tmpPath,
            std::ios::out | std::ios::trunc | std::ios::binary);
        if (!tmp.is_open())
            throw std::runtime_error(
                "BinaryRecordFile: cannot create migration temp file for " + path_);

        char header[kHeaderSize] = {};
        static constexpr char kMagic[8] = {'A','C','C','T','P','R','O','\0'};
        std::memcpy(header, kMagic, 8);
        const uint16_t ver = kVersion;
        std::memcpy(header + 8, &ver, 2);
        const uint16_t rs = static_cast<uint16_t>(recordSize_);
        std::memcpy(header + 10, &rs, 2);
        // lastWriteId stays 0

        tmp.write(header, kHeaderSize);
        tmp.write(data.data(), static_cast<std::streamsize>(oldSize));
        tmp.flush();
    }

    // Close before renaming — Windows won't replace an open file handle.
    file_.close();

    // Atomic rename over the original file.
    std::error_code ec;
    std::filesystem::rename(tmpPath, path_, ec);
    if (ec)
        throw std::runtime_error(
            "BinaryRecordFile: migration rename failed for " + path_
            + ": " + ec.message());

    // Re-open the now-migrated file.
    if (!openFile())
        throw std::runtime_error(
            "BinaryRecordFile: re-open after migration failed for " + path_);

    lastWriteId_ = 0;
}

// ── Schema migration ─────────────────────────────────────────────────────────
// Finish or roll back a migration that a previous process crashed in the middle of.
// Runs FIRST in the ctor, before the file is interpreted. Ordering of the migration
// steps (see performSchemaMigration): S1 write+fsync temp, S2 rename original→bak,
// S3 rename temp→path, S4 delete bak. Each crash window maps to a recoverable state.
void BinaryRecordFile::recoverInterruptedMigration()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string tmp = migratingPath();
    const std::string bak = migrateBakPath();
    const bool hasTmp  = fs::exists(tmp, ec);
    const bool hasPath = fs::exists(path_, ec);
    const bool hasBak  = fs::exists(bak, ec);

    if (hasTmp && hasPath) {
        // Crashed at/before S2: the original is intact at path. The temp is
        // unverified → discard it; migration re-runs cleanly on this open.
        fs::remove(tmp, ec);
        if (hasBak) fs::remove(bak, ec);
        return;
    }
    if (hasTmp && !hasPath) {
        // Crashed between S2 and S3: original is in .bak, the migrated file IS the
        // temp. Finish forward — install the temp, then drop the backup.
        fs::rename(tmp, path_, ec);
        if (!ec) { fs::remove(bak, ec); migrated_ = true; }
        return;
    }
    if (!hasTmp && !hasPath && hasBak) {
        // Path gone with no temp but a backup present: restore the original (the
        // migration never produced an installable result). Never lose data.
        fs::rename(bak, path_, ec);
        return;
    }
    if (!hasTmp && hasPath && hasBak) {
        // Crashed between S3 and S4: path is already migrated; the backup is the
        // stale original → clean it up.
        fs::remove(bak, ec);
        return;
    }
    // Otherwise: no migration artifacts to recover.
}

// Decide whether to migrate, refuse (downgrade), or pass through (already current).
void BinaryRecordFile::maybeMigrateSchema()
{
    if (schemaOnDisk_ > schemaTarget_) {
        throw std::runtime_error(
            "BinaryRecordFile: file schema v" + std::to_string(schemaOnDisk_)
            + " is newer than this build (v" + std::to_string(schemaTarget_)
            + ") — refusing to open " + path_ + " to avoid corrupting it");
    }
    if (schemaOnDisk_ == schemaTarget_) {
        // Same schema must mean same record size; otherwise the layout was changed
        // without bumping schemaVersion — fail loudly rather than misread records.
        if (recordSize_ != targetRecordSize_)
            throw std::runtime_error(
                "BinaryRecordFile: schema v" + std::to_string(schemaOnDisk_)
                + " record-size mismatch (file " + std::to_string(recordSize_)
                + ", code " + std::to_string(targetRecordSize_)
                + ") — bump schemaVersion when changing the layout, in " + path_);
        return;   // up to date
    }
    performSchemaMigration(schemaOnDisk_, recordSize_, targetRecordSize_);
}

// Forward-migrate every record from `oldSize` (schema `fromSchema`) to `newSize`
// (schemaTarget_). Atomic + crash-safe + data-preserving.
void BinaryRecordFile::performSchemaMigration(uint16_t fromSchema,
                                              std::size_t oldSize,
                                              std::size_t newSize)
{
    file_.seekg(0, std::ios::end);
    const auto fileSize = static_cast<std::size_t>(file_.tellg());
    const std::size_t n = (fileSize >= kHeaderSize) ? (fileSize - kHeaderSize) / oldSize : 0;

    // ── S1: write the fully-migrated data to a temp file, then fsync it. ─────────
    const std::string tmp = migratingPath();
    {
        char header[kHeaderSize] = {};
        static constexpr char kMagic[8] = {'A','C','C','T','P','R','O','\0'};
        std::memcpy(header, kMagic, 8);
        const uint16_t ver = kVersion;                       std::memcpy(header + 8,  &ver, 2);
        const uint16_t rs  = static_cast<uint16_t>(newSize); std::memcpy(header + 10, &rs,  2);
        const uint16_t sc  = schemaTarget_;                  std::memcpy(header + 12, &sc,  2);
        std::memcpy(header + 16, &lastWriteId_, 8);          // preserve the journal counter

        FILE* outF = std::fopen(tmp.c_str(), "wb");
        if (!outF)
            throw std::runtime_error("BinaryRecordFile: cannot create migration temp " + tmp);

        bool ok = std::fwrite(header, 1, kHeaderSize, outF) == kHeaderSize;

        std::vector<char> oldRec(oldSize);
        for (std::size_t i = 0; ok && i < n; ++i) {
            file_.seekg(dataOffset(static_cast<uint32_t>(i)));   // recordSize_ == oldSize here
            file_.read(oldRec.data(), static_cast<std::streamsize>(oldSize));
            if (static_cast<std::size_t>(file_.gcount()) != oldSize) { ok = false; break; }

            std::vector<char> newRec;
            if (migrate_) {
                newRec = migrate_(fromSchema, oldRec.data(), oldSize, newSize);
            } else {
                // Default: zero-extend (append-only schema discipline).
                newRec.assign(newSize, 0);
                std::memcpy(newRec.data(), oldRec.data(), std::min(oldSize, newSize));
            }
            if (newRec.size() != newSize) {
                std::fclose(outF);
                std::error_code rmec; std::filesystem::remove(tmp, rmec);
                throw std::runtime_error(
                    "BinaryRecordFile: migration produced a " + std::to_string(newRec.size())
                    + "-byte record (expected " + std::to_string(newSize) + ") for " + path_);
            }
            ok = std::fwrite(newRec.data(), 1, newSize, outF) == newSize;
        }
        if (ok) std::fflush(outF);
        if (ok) PLATFORM_FSYNC(fileno(outF));   // temp durable before we touch the original
        std::fclose(outF);
        if (!ok) {
            std::error_code rmec; std::filesystem::remove(tmp, rmec);
            throw std::runtime_error("BinaryRecordFile: migration write failed for " + path_);
        }
    }
    acctMaybeCrash("afterMigrationTmp");      // temp durable, original untouched

    file_.close();   // Windows won't rename over an open handle

    // ── S2: move the original aside (backup). ────────────────────────────────────
    std::error_code ec;
    std::filesystem::rename(path_, migrateBakPath(), ec);
    if (ec) {
        openFile();
        throw std::runtime_error(
            "BinaryRecordFile: migration backup rename failed for " + path_ + ": " + ec.message());
    }
    acctMaybeCrash("afterMigrationBackup");   // original safe in .bak, path absent

    // ── S3: install the migrated file. ───────────────────────────────────────────
    std::filesystem::rename(tmp, path_, ec);
    if (ec) {
        std::error_code ec2;
        std::filesystem::rename(migrateBakPath(), path_, ec2);   // roll back
        openFile();
        throw std::runtime_error(
            "BinaryRecordFile: migration install rename failed for " + path_ + ": " + ec.message());
    }
    acctMaybeCrash("afterMigrationRename");   // path = migrated; .bak still present

    // ── S4: success — drop the backup, re-open at the new layout. ────────────────
    std::filesystem::remove(migrateBakPath(), ec);
    if (!openFile())
        throw std::runtime_error("BinaryRecordFile: re-open after migration failed for " + path_);
    readHeader();                  // reloads recordSize_/schemaOnDisk_/lastWriteId_
    recordSize_   = newSize;
    schemaOnDisk_ = schemaTarget_;
    migrated_     = true;
}

// ── Journal write ────────────────────────────────────────────────────────────
void BinaryRecordFile::writeJournal(const char* record,
                                     uint32_t    targetId,
                                     uint64_t    writeId)
{
    const std::string jpath = journalPath();
    // CRC covers the record AND the metadata (targetId, writeId): a corrupted
    // targetId would otherwise replay the record to the WRONG offset undetected.
    std::vector<char> crcBuf(recordSize_ + 12);
    std::memcpy(crcBuf.data(),                  record,    recordSize_);
    std::memcpy(crcBuf.data() + recordSize_,     &targetId, 4);
    std::memcpy(crcBuf.data() + recordSize_ + 4, &writeId,  8);
    const uint32_t checksum = crc32(crcBuf.data(), crcBuf.size());

    // Write using a C FILE* so we can call fsync via fileno().
    FILE* jf = std::fopen(jpath.c_str(), "wb");
    if (!jf)
        throw std::runtime_error(
            "BinaryRecordFile: cannot open journal for writing: " + jpath);

    bool ok =
        std::fwrite(record,    1, recordSize_, jf) == recordSize_
     && std::fwrite(&targetId, 4, 1, jf) == 1
     && std::fwrite(&checksum, 4, 1, jf) == 1
     && std::fwrite(&writeId,  8, 1, jf) == 1;

    if (ok) std::fflush(jf);
    // Platform fsync so the journal is durable before we touch the main file.
    if (ok) PLATFORM_FSYNC(fileno(jf));

    std::fclose(jf);

    if (!ok)
        throw std::runtime_error(
            "BinaryRecordFile: journal write failed: " + jpath);
}

// ── Journal replay ───────────────────────────────────────────────────────────
void BinaryRecordFile::replayJournal()
{
    const std::string jpath = journalPath();
    FILE* jf = std::fopen(jpath.c_str(), "rb");
    if (!jf) return;   // no journal

    const std::size_t journalSize = recordSize_ + 16;
    std::vector<char> buf(journalSize);

    bool valid = (std::fread(buf.data(), 1, journalSize, jf) == journalSize);
    std::fclose(jf);

    if (!valid) { deleteJournal(); return; }

    uint32_t targetId = 0;
    uint32_t storedCrc = 0;
    uint64_t writeId   = 0;
    std::memcpy(&targetId,  buf.data() + recordSize_,     4);
    std::memcpy(&storedCrc, buf.data() + recordSize_ + 4, 4);
    std::memcpy(&writeId,   buf.data() + recordSize_ + 8, 8);

    // Re-CRC the record + metadata (targetId, writeId) exactly as writeJournal did.
    std::vector<char> crcBuf(recordSize_ + 12);
    std::memcpy(crcBuf.data(),                  buf.data(), recordSize_);
    std::memcpy(crcBuf.data() + recordSize_,     &targetId,  4);
    std::memcpy(crcBuf.data() + recordSize_ + 4, &writeId,   8);
    const uint32_t actualCrc = crc32(crcBuf.data(), crcBuf.size());
    if (actualCrc != storedCrc) { deleteJournal(); return; }

    // Already committed?
    if (writeId <= lastWriteId_) { deleteJournal(); return; }

    // A legitimate in-flight write targets an existing slot (update) or exactly the tail
    // (append), i.e. targetId ∈ [0, count()]. A journal targeting BEYOND the end — from
    // corruption or a crafted (CRC-forged) journal — would seek far past EOF and write there,
    // ballooning the file (sparse growth / disk-fill). Refuse it, same as a CRC mismatch.
    if (static_cast<std::size_t>(targetId) > count()) { deleteJournal(); return; }

    // Apply the write to the main file.
    file_.seekp(dataOffset(targetId));
    file_.write(buf.data(), static_cast<std::streamsize>(recordSize_));
    file_.flush();

    // Commit the writeId to the header.
    lastWriteId_ = writeId;
    writeHeader();

    // Make the recovered record durable before dropping the journal — otherwise a
    // power cut mid-recovery could delete the journal while the replayed write is
    // still only in OS cache, losing it a second time with no copy left.
    syncToDisk();

    deleteJournal();
    recovered_ = true;   // a crash-leftover journal was replayed
}

void BinaryRecordFile::deleteJournal()
{
    std::error_code ec;
    std::filesystem::remove(journalPath(), ec);
    // Ignore errors: missing journal is fine.
}

// ── Durable sync of the main file ────────────────────────────────────────────
// The main file is an fstream, which only flushes userspace → OS cache; on a
// power cut those pages can be lost. fstream exposes no file descriptor, so we
// open a second handle by path and fsync THAT — the OS page cache is per-file, so
// flushing any handle flushes the file's dirty pages to stable storage. This is
// the barrier that lets us delete the journal safely: once it returns true, the
// committed write survives power loss; if the durable handle can't be opened we
// degrade to the stream flush (still crash-consistent — the journal protects
// against torn records — but the last write may not survive an abrupt power cut).
bool BinaryRecordFile::syncToDisk()
{
    file_.flush();
    // "rb+" — open existing for update WITHOUT truncating; we only need a handle
    // to fsync, not to write through.
    FILE* f = std::fopen(path_.c_str(), "rb+");
    if (!f) return false;
    const int rc = PLATFORM_FSYNC(fileno(f));
    std::fclose(f);
    return rc == 0;
}

// ── Core operations ──────────────────────────────────────────────────────────
std::size_t BinaryRecordFile::count()
{
    file_.seekg(0, std::ios::end);
    const auto endPos = file_.tellg();
    if (endPos < static_cast<std::streampos>(kHeaderSize)) {
        file_.clear();
        return 0;
    }
    return (static_cast<std::size_t>(endPos) - kHeaderSize) / recordSize_;
}

uint32_t BinaryRecordFile::append(const char* buffer)
{
    const uint32_t newId = static_cast<uint32_t>(count());
    const uint64_t wid   = lastWriteId_ + 1;

    // 1. Write journal (durable).
    writeJournal(buffer, newId, wid);
    acctMaybeCrash("afterJournal");   // journal durable, main file untouched

    // 2. Write record to main file.
    file_.seekp(dataOffset(newId));
    file_.write(buffer, static_cast<std::streamsize>(recordSize_));
    if (!file_.good()) {
        file_.clear();
        throw std::runtime_error("BinaryRecordFile: write failed in append");
    }
    file_.flush();
    acctMaybeCrash("afterMainWrite"); // record in OS cache, header not yet committed

    // 3. Commit: update monotonic counter in header.
    lastWriteId_ = wid;
    writeHeader();
    acctMaybeCrash("afterHeader");    // committed but journal not yet deleted (stale)

    // 3b. Make the record + header durable BEFORE dropping the journal, so a power
    //     cut can never delete our only recovery copy while the data is still cached.
    syncToDisk();

    // 4. Journal no longer needed.
    deleteJournal();

    return newId;
}

bool BinaryRecordFile::read(uint32_t id, char* buffer)
{
    if (id >= count()) {
        std::memset(buffer, 0, recordSize_);
        return false;
    }
    file_.seekg(dataOffset(id));
    file_.read(buffer, static_cast<std::streamsize>(recordSize_));
    if (static_cast<std::size_t>(file_.gcount()) != recordSize_) {
        std::memset(buffer, 0, recordSize_);
        file_.clear();
        return false;
    }
    return true;
}

bool BinaryRecordFile::update(uint32_t id, const char* buffer)
{
    if (id >= count()) return false;

    const uint64_t wid = lastWriteId_ + 1;

    // 1. Write journal (durable).
    writeJournal(buffer, id, wid);
    acctMaybeCrash("afterJournal");   // journal durable, main record still old

    // 2. Overwrite record in main file.
    file_.seekp(dataOffset(id));
    file_.write(buffer, static_cast<std::streamsize>(recordSize_));
    if (!file_.good()) {
        file_.clear();
        return false;
    }
    file_.flush();
    acctMaybeCrash("afterMainWrite"); // record in OS cache, header not yet committed

    // 3. Commit.
    lastWriteId_ = wid;
    writeHeader();
    acctMaybeCrash("afterHeader");    // committed but journal not yet deleted (stale)

    // 3b. Durable barrier before dropping the journal (see append / syncToDisk).
    syncToDisk();

    // 4. Clean up journal.
    deleteJournal();

    return true;
}

// ── Clear (projection rebuild) ───────────────────────────────────────────────
void BinaryRecordFile::clear()
{
    deleteJournal();
    file_.close();
    std::error_code ec;
    std::filesystem::resize_file(path_, kHeaderSize, ec);   // keep header, drop records
    if (!openFile())
        throw std::runtime_error("BinaryRecordFile: re-open after clear failed for " + path_);
    lastWriteId_ = 0;
    writeHeader();   // record size + schema preserved, lastWriteId reset
    syncToDisk();
}

// ── Content fingerprint (projection verification) ────────────────────────────
uint32_t BinaryRecordFile::contentHash()
{
    const std::size_t n = count();
    if (n == 0) return crc32(nullptr, 0);   // empty region → fixed value (0)
    const std::size_t bytes = n * recordSize_;
    std::vector<char> buf(bytes);
    file_.seekg(dataOffset(0));
    file_.read(buf.data(), static_cast<std::streamsize>(bytes));
    const std::size_t got = static_cast<std::size_t>(file_.gcount());
    file_.clear();
    return crc32(buf.data(), got);
}

// ── Compaction ───────────────────────────────────────────────────────────────
std::vector<uint32_t> BinaryRecordFile::compact(
    std::function<void(std::size_t, std::size_t)> progressFn)
{
    const std::size_t total = count();
    std::vector<uint32_t> idMap(total, UINT32_MAX);

    const std::string tmpPath = path_ + ".compact_tmp";
    {
        // Write live records into a temp file with a fresh header.
        std::fstream tmp(tmpPath,
            std::ios::out | std::ios::trunc | std::ios::binary);
        if (!tmp.is_open())
            throw std::runtime_error(
                "BinaryRecordFile: compact cannot create temp file " + tmpPath);

        char header[kHeaderSize] = {};
        static constexpr char kMagic[8] = {'A','C','C','T','P','R','O','\0'};
        std::memcpy(header, kMagic, 8);
        const uint16_t ver = kVersion;
        std::memcpy(header + 8, &ver, 2);
        const uint16_t rs = static_cast<uint16_t>(recordSize_);
        std::memcpy(header + 10, &rs, 2);
        tmp.write(header, kHeaderSize);

        std::vector<char> buf(recordSize_);
        uint32_t newId = 0;
        for (std::size_t i = 0; i < total; ++i) {
            if (progressFn) progressFn(i, total);
            if (!read(static_cast<uint32_t>(i), buf.data())) continue;

            const unsigned char flag =
                static_cast<unsigned char>(buf[deletedFlagOffset_]);
            if (flag != 0) continue;   // soft-deleted — skip

            tmp.write(buf.data(), static_cast<std::streamsize>(recordSize_));
            idMap[i] = newId++;
        }
        tmp.flush();
    }

    // Atomic rename over the original file.
    file_.close();

    std::error_code ec;
    std::filesystem::rename(tmpPath, path_, ec);
    if (ec) {
        openFile();   // best-effort reopen
        readHeader();
        throw std::runtime_error(
            "BinaryRecordFile: compact rename failed: " + ec.message());
    }

    // Re-open and reload.
    if (!openFile())
        throw std::runtime_error(
            "BinaryRecordFile: re-open after compact failed for " + path_);
    lastWriteId_ = 0;
    readHeader();

    if (progressFn) progressFn(total, total);
    return idMap;
}
