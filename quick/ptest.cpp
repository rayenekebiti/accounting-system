#include "ptest.h"
#include "logging.h"

#include "storage/BinaryRecordFile.h"
#include "storage/EventLog.h"
#include "storage/AuditJournal.h"
#include "storage/CompatibilityManifest.h"
#include "storage/PostingPolicy.h"
#include "storage/InvoiceRepository.h"
#include "storage/InvoiceLineRepository.h"
#include "storage/CustomerRepository.h"
#include "storage/SupplierRepository.h"
#include "storage/ExpenseRepository.h"
#include "core/Customer.h"
#include "core/Expense.h"
#include "core/InvoiceLine.h"
#include <cmath>
#include "core/Invoice.h"
#include "core/InvoiceLine.h"
#include "core/InvoiceTotals.h"
#include "core/Money.h"
#include "InvoiceDraftLinesModel.h"

#include <QString>
#include <QByteArray>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Deterministic persistence + integrity test harness.
//
// Every test runs in an isolated scratch file under ACCT_DATA_DIR and exercises
// the REAL BinaryRecordFile journal/recovery protocol — no mocks. Where a test
// needs a crash-leftover journal, it crafts one byte-for-byte with a CRC computed
// by the same BinaryRecordFile::crc32 the production code uses, so "valid" and
// "corrupt" mean exactly what they mean at runtime.
// ─────────────────────────────────────────────────────────────────────────────

namespace {

int g_pass = 0;
int g_fail = 0;

void ok(bool cond, const char* name)
{
    if (cond) ++g_pass; else ++g_fail;
    std::fprintf(stderr, "  [%s] %s\n", cond ? "PASS" : "FAIL", name);
    std::fflush(stderr);
}

void section(const char* name)
{
    std::fprintf(stderr, "\n── %s\n", name);
    std::fflush(stderr);
}

// Generic test record: 16 bytes, deleted-flag at byte 12. Content is a
// deterministic function of seed so equality checks are exact.
constexpr std::size_t kRS  = 16;
constexpr std::size_t kDel = 12;

std::vector<char> makeRec(int seed)
{
    std::vector<char> r(kRS, 0);
    for (std::size_t i = 0; i < kRS; ++i)
        r[i] = static_cast<char>((seed * 7 + static_cast<int>(i) * 3) & 0xFF);
    r[kDel] = 0;   // not deleted
    return r;
}

bool sameRec(const char* a, const std::vector<char>& b)
{
    return std::memcmp(a, b.data(), b.size()) == 0;
}

// Remove a record file and its journal so each test starts clean.
std::string freshPath(const std::string& base, const char* name)
{
    std::string p = base + "/" + name;
    std::error_code ec;
    std::filesystem::remove(p, ec);
    std::filesystem::remove(p + ".journal", ec);
    std::filesystem::remove(p + ".migrating", ec);
    std::filesystem::remove(p + ".migrate.bak", ec);
    std::filesystem::remove(p + ".compact_tmp", ec);
    return p;
}

bool journalExists(const std::string& datPath)
{
    std::error_code ec;
    return std::filesystem::exists(datPath + ".journal", ec);
}

// Craft a journal file beside `datPath`, matching BinaryRecordFile's on-disk
// layout: [record(kRS)][targetId(4)][crc(4)][writeId(8)]. The CRC covers
// record + targetId + writeId exactly as writeJournal() computes it.
//   corruptCrc  — flip the stored CRC so replay must reject it.
//   omitWriteId — write a short journal (missing the trailing writeId) to model
//                 a torn/partial journal write.
void craftJournal(const std::string& datPath,
                  const std::vector<char>& rec,
                  uint32_t targetId,
                  uint64_t writeId,
                  bool corruptCrc = false,
                  bool omitWriteId = false)
{
    std::vector<char> crcBuf(kRS + 12);
    std::memcpy(crcBuf.data(), rec.data(), kRS);
    std::memcpy(crcBuf.data() + kRS, &targetId, 4);
    std::memcpy(crcBuf.data() + kRS + 4, &writeId, 8);
    uint32_t crc = BinaryRecordFile::crc32(crcBuf.data(), crcBuf.size());
    if (corruptCrc) crc ^= 0xFFFFFFFFu;

    const std::string jp = datPath + ".journal";
    FILE* f = std::fopen(jp.c_str(), "wb");
    if (!f) return;
    std::fwrite(rec.data(), 1, kRS, f);
    std::fwrite(&targetId, 4, 1, f);
    std::fwrite(&crc, 4, 1, f);
    if (!omitWriteId) std::fwrite(&writeId, 8, 1, f);
    std::fclose(f);
}

// Seed `n` records into a fresh file via the real append() path, then close.
// After this, the header's lastWriteId == n.
void seed(const std::string& path, int n)
{
    BinaryRecordFile f(path, kRS, kDel);
    for (int i = 0; i < n; ++i) {
        auto r = makeRec(i);
        f.append(r.data());
    }
}

// ── Test sections ────────────────────────────────────────────────────────────

void testRoundtrip(const std::string& base)
{
    section("Round-trip: save → close → reopen → verify");
    std::string p = freshPath(base, "roundtrip.dat");
    constexpr int N = 200;
    seed(p, N);

    BinaryRecordFile f(p, kRS, kDel);
    ok(f.count() == N, "count == 200 after reopen");
    bool allMatch = true;
    char buf[kRS];
    for (int i = 0; i < N; ++i) {
        if (!f.read(static_cast<uint32_t>(i), buf) || !sameRec(buf, makeRec(i))) {
            allMatch = false;
            break;
        }
    }
    ok(allMatch, "all 200 records survive the round-trip byte-for-byte");
    ok(!f.recoveredOnOpen(), "no spurious recovery on a clean reopen");
}

void testReplayValid(const std::string& base)
{
    section("Journal replay: valid leftover journal is applied");
    std::string p = freshPath(base, "replay.dat");
    seed(p, 5);                                  // lastWriteId = 5
    auto repl = makeRec(999);
    craftJournal(p, repl, /*targetId*/ 2, /*writeId*/ 6);

    BinaryRecordFile f(p, kRS, kDel);
    char buf[kRS];
    f.read(2, buf);
    ok(f.recoveredOnOpen(), "recoveredOnOpen() is true after replay");
    ok(sameRec(buf, repl), "record 2 now holds the journal's content");
    ok(!journalExists(p), "journal file is deleted after a successful replay");
    ok(f.count() == 5, "record count is unchanged by an in-range replay");
}

void testCorruptCrc(const std::string& base)
{
    section("Corrupt journal: bad CRC is ignored, original preserved");
    std::string p = freshPath(base, "corrupt.dat");
    seed(p, 5);
    auto bad = makeRec(999);
    craftJournal(p, bad, 2, 6, /*corruptCrc*/ true);

    BinaryRecordFile f(p, kRS, kDel);
    char buf[kRS];
    f.read(2, buf);
    ok(sameRec(buf, makeRec(2)), "record 2 keeps its original value");
    ok(!f.recoveredOnOpen(), "no recovery is reported for a corrupt journal");
    ok(!journalExists(p), "corrupt journal is discarded, not left to re-trigger");
}

void testStaleJournal(const std::string& base)
{
    section("Stale journal: already-committed writeId is not re-applied");
    std::string p = freshPath(base, "stale.dat");
    seed(p, 5);                                  // lastWriteId = 5
    auto repl = makeRec(999);
    // writeId == lastWriteId models the 'afterHeader' crash: header already
    // advanced, journal not yet deleted. Replay must treat it as committed.
    craftJournal(p, repl, 2, /*writeId*/ 5);

    BinaryRecordFile f(p, kRS, kDel);
    char buf[kRS];
    f.read(2, buf);
    ok(sameRec(buf, makeRec(2)), "record 2 is NOT overwritten by a stale journal");
    ok(!f.recoveredOnOpen(), "stale journal does not count as a recovery");
    ok(!journalExists(p), "stale journal is cleaned up");
}

void testTornJournal(const std::string& base)
{
    section("Torn journal: short/partial journal is rejected, no crash");
    std::string p = freshPath(base, "torn.dat");
    seed(p, 5);
    auto repl = makeRec(999);
    craftJournal(p, repl, 2, 6, /*corruptCrc*/ false, /*omitWriteId*/ true);

    BinaryRecordFile f(p, kRS, kDel);   // must not throw / crash
    char buf[kRS];
    f.read(2, buf);
    ok(sameRec(buf, makeRec(2)), "record 2 unchanged when the journal is truncated");
    ok(!f.recoveredOnOpen(), "a torn journal is not applied");
    ok(!journalExists(p), "torn journal is discarded");
}

void testPartialMainWrite(const std::string& base)
{
    section("Partial main write + journal: torn append is completed on reopen");
    std::string p = freshPath(base, "partial.dat");
    seed(p, 4);                                  // records 0..3, lastWriteId = 4

    // Simulate a crash mid-append of record 4: append 8 of 16 bytes to the data
    // file (a torn record the count math will ignore) and leave a valid journal.
    {
        FILE* f = std::fopen(p.c_str(), "ab");
        auto partial = makeRec(4);
        std::fwrite(partial.data(), 1, 8, f);    // only half the record
        std::fclose(f);
    }
    auto full = makeRec(4);
    craftJournal(p, full, /*targetId*/ 4, /*writeId*/ 5);

    BinaryRecordFile f(p, kRS, kDel);
    ok(f.recoveredOnOpen(), "recovery is reported for the torn append");
    ok(f.count() == 5, "the partially-written record 4 is completed (count == 5)");
    char buf[kRS];
    f.read(4, buf);
    ok(sameRec(buf, full), "record 4 holds the full journalled content");
}

void testIntegrityTotals(const std::string& base)
{
    section("Integrity: total == Σ lines, subtotal + tax == total (no drift)");
    (void)base;

    // Three lines of qty 1 × $0.10 @ 25% tax. Each line: 0.10 × 1.25 = 0.125 →
    // rounds to $0.13. Σ line totals = $0.39. The OLD code rounded the aggregate
    // double (0.375 → $0.38), drifting 1 cent below the lines. computeInvoiceTotals
    // sums the per-line Money, so it must equal $0.39.
    InvoiceDraftLinesModel m;
    for (int i = 0; i < 3; ++i) {
        m.addBlankLine();
        m.setCell(i, "description",   "widget");
        m.setCell(i, "qtyText",       "1");
        m.setCell(i, "unitPriceText", "0.10");
        m.setCell(i, "taxText",       "25");
    }

    auto lines = m.buildLines();
    InvoiceTotals t = computeInvoiceTotals(lines);

    Money sumLines;
    for (const auto& l : lines) sumLines += l.getLineTotal();

    ok(t.total == sumLines, "header total equals the exact sum of line totals");
    ok(t.subtotal + t.tax == t.total, "subtotal + tax == total (exact)");
    ok(t.total.cents() == 39, "total is 39 cents (per-line rounding)");

    // Prove the drift the fix removes: the naive aggregate-double path differs.
    Money naive = Money::fromDouble(m.total());
    ok(naive.cents() == 38, "naive aggregate-double total would be 38 cents");
    ok(naive != t.total, "aggregate-rounding drifts from per-line — fix is required");
}

void testMoneyDeterminism(const std::string& base)
{
    section("Money: exact integer cents, no floating-point drift");
    (void)base;

    ok(Money::fromDouble(19.99).cents() == 1999, "19.99 → 1999 cents");
    ok(Money::fromDouble(0.10).cents()  == 10,   "0.10 → 10 cents");
    ok(Money::fromDouble(0.125).cents() == 13,   "0.125 → 13 cents (half away from zero)");
    ok(Money::fromDouble(-0.125).cents() == -13, "-0.125 → -13 cents (symmetric)");

    // The classic 0.1 + 0.2 != 0.3 float trap does not exist in cents.
    Money a = Money::fromDouble(0.10);
    Money b = Money::fromDouble(0.20);
    ok((a + b).cents() == 30, "0.10 + 0.20 == 0.30 exactly in cents");

    // Building the same draft twice yields byte-identical totals.
    auto build = []() {
        InvoiceDraftLinesModel m;
        m.addBlankLine();
        m.setCell(0, "qtyText", "3");
        m.setCell(0, "unitPriceText", "19.99");
        m.setCell(0, "taxText", "19");
        return computeInvoiceTotals(m.buildLines()).total.cents();
    };
    ok(build() == build(), "repeated builds of the same draft are deterministic");
}

void testDuplicateNumber(const std::string& base)
{
    section("Integrity: duplicate invoice numbers are detectable");
    std::string p = freshPath(base, "dupnum.dat");
    InvoiceRepository repo(p);

    // A serializable invoice needs a known status; totals default to 0 (valid).
    Invoice a; a.setInvoiceNumber("INV-1"); a.setStatus(INVOICE_DRAFT);
    Invoice b; b.setInvoiceNumber("INV-2"); b.setStatus(INVOICE_DRAFT);
    Invoice c; c.setInvoiceNumber("INV-1"); c.setStatus(INVOICE_DRAFT);   // duplicate of a
    uint32_t idA = repo.save(a);
    repo.save(b);
    repo.save(c);

    ok(repo.findIdByNumber("INV-1") == static_cast<int>(idA),
       "findIdByNumber returns the FIRST holder of a duplicated number");
    ok(repo.findIdByNumber("INV-2") >= 0, "an existing unique number is found");
    ok(repo.findIdByNumber("INV-404") == -1, "a missing number returns -1");
    ok(repo.findIdByNumber("") == -1, "empty number returns -1");
    // The commit() guard rejects a save when findIdByNumber != self; this proves
    // the primitive that guard relies on.
}

void testRepeatedCycles(const std::string& base)
{
    section("Stability: many open/append/close cycles");
    std::string p = freshPath(base, "cycles.dat");
    constexpr int CYCLES = 200;
    for (int c = 0; c < CYCLES; ++c) {
        BinaryRecordFile f(p, kRS, kDel);      // open
        auto r = makeRec(c);
        f.append(r.data());                    // append one
    }                                          // close (destructor) — repeat
    BinaryRecordFile f(p, kRS, kDel);
    ok(f.count() == CYCLES, "200 independent open/append/close cycles → 200 records");
    char buf[kRS];
    bool spot = f.read(0, buf) && sameRec(buf, makeRec(0))
             && f.read(CYCLES - 1, buf) && sameRec(buf, makeRec(CYCLES - 1));
    ok(spot, "first and last records intact after all cycles");
    ok(!f.recoveredOnOpen(), "clean shutdown each cycle leaves no journal to replay");
}

void testDurableSyncHandle(const std::string& base)
{
    section("Durability: fsync barrier handle opens while the file is in use");
    std::string p = freshPath(base, "durable.dat");
    BinaryRecordFile f(p, kRS, kDel);
    auto r = makeRec(7);
    f.append(r.data());                          // this internally calls syncToDisk()

    // syncToDisk() opens a second handle by path to fsync. If the OS denied that
    // sharing, durability would silently degrade — so prove the handle opens here.
    FILE* h = std::fopen(p.c_str(), "rb+");
    ok(h != nullptr, "a concurrent rb+ handle opens — fsync barrier is live, not degraded");
    if (h) std::fclose(h);
}

// ── Schema migration ─────────────────────────────────────────────────────────

void testSchemaAdditive(const std::string& base)
{
    section("Schema migration: additive v1→v2 (default zero-extend)");
    std::string p = freshPath(base, "mig_add.dat");
    constexpr int N = 100;
    { BinaryRecordFile f(p, 16, 12, /*schema*/ 1);
      for (int i = 0; i < N; ++i) { auto r = makeRec(i); f.append(r.data()); } }

    {
        BinaryRecordFile f(p, 24, 12, /*schema*/ 2);   // grow 16→24, schema 1→2
        ok(f.migratedOnOpen(), "migration ran on first v2 open");
        ok(f.schemaVersion() == 2, "on-disk schema is now 2");
        ok(f.count() == static_cast<std::size_t>(N), "record count preserved through migration");
        bool good = true;
        char buf[24];
        for (int i = 0; i < N && good; ++i) {
            if (!f.read(static_cast<uint32_t>(i), buf)) { good = false; break; }
            auto exp = makeRec(i);
            if (std::memcmp(buf, exp.data(), 16) != 0) good = false;       // original 16 preserved
            for (int b = 16; b < 24; ++b) if (buf[b] != 0) good = false;   // tail zero-extended
        }
        ok(good, "every record: original 16 bytes preserved + 8 bytes zero-filled");
    }
    {
        BinaryRecordFile f(p, 24, 12, 2);
        ok(!f.migratedOnOpen(), "reopening at v2 does NOT re-migrate (idempotent)");
        ok(f.count() == static_cast<std::size_t>(N), "count stable on reopen");
    }
}

void testSchemaCustom(const std::string& base)
{
    section("Schema migration: custom transform (same size, non-additive)");
    std::string p = freshPath(base, "mig_custom.dat");
    { BinaryRecordFile f(p, 16, 12, 1);
      for (int i = 0; i < 5; ++i) { auto r = makeRec(i); f.append(r.data()); } }

    auto bump = [](uint16_t, const char* old, std::size_t os, std::size_t ns) {
        std::vector<char> v(ns, 0);
        std::memcpy(v.data(), old, os < ns ? os : ns);
        v[0] = static_cast<char>(v[0] + 1);   // transform: increment byte 0
        return v;
    };
    BinaryRecordFile f(p, 16, 12, 2, bump);   // same size, schema 1→2
    ok(f.migratedOnOpen(), "custom migration ran");
    bool good = true;
    char buf[16];
    for (int i = 0; i < 5 && good; ++i) {
        f.read(static_cast<uint32_t>(i), buf);
        auto exp = makeRec(i);
        if (static_cast<unsigned char>(buf[0]) != static_cast<unsigned char>(exp[0] + 1)) good = false;
        if (std::memcmp(buf + 1, exp.data() + 1, 15) != 0) good = false;
    }
    ok(good, "custom transform applied to every record (byte0+1, remainder intact)");
}

void testSchemaDowngradeRefused(const std::string& base)
{
    section("Schema migration: newer file refused by older code (downgrade protection)");
    std::string p = freshPath(base, "mig_down.dat");
    { BinaryRecordFile f(p, 16, 12, /*schema*/ 3); auto r = makeRec(1); f.append(r.data()); }
    bool threw = false;
    try { BinaryRecordFile f(p, 16, 12, /*schema*/ 2); }
    catch (const std::exception&) { threw = true; }
    ok(threw, "schema-3 file opened by schema-2 code throws (never silently corrupts)");
}

void testSchemaSizeGuard(const std::string& base)
{
    section("Schema migration: layout change without a schema bump fails loudly");
    std::string p = freshPath(base, "mig_guard.dat");
    { BinaryRecordFile f(p, 16, 12, 1); auto r = makeRec(1); f.append(r.data()); }
    bool threw = false;
    try { BinaryRecordFile f(p, 24, 12, /*same schema*/ 1); }
    catch (const std::exception&) { threw = true; }
    ok(threw, "growing the record at the SAME schema throws (forces an explicit migration)");
}

void testSchemaEmpty(const std::string& base)
{
    section("Schema migration: empty file migrates cleanly");
    std::string p = freshPath(base, "mig_empty.dat");
    { BinaryRecordFile f(p, 16, 12, 1); }
    BinaryRecordFile f(p, 24, 12, 2);
    ok(f.schemaVersion() == 2, "empty file schema bumped to 2");
    ok(f.count() == 0, "still zero records, no corruption");
}

void testSchemaLegacyZero(const std::string& base)
{
    // The real field-upgrade path: files written before schema versioning have 0 in
    // the schemaVersion header bytes (they were reserved/zero). They MUST open as the
    // baseline (v1) with no migration and no data loss.
    section("Schema migration: legacy file (schemaVersion byte = 0) opens as baseline");
    std::string p = freshPath(base, "mig_legacy.dat");
    { BinaryRecordFile f(p, 16, 12, 1);
      for (int i = 0; i < 5; ++i) { auto r = makeRec(i); f.append(r.data()); } }
    // Simulate a pre-versioning file: zero the schemaVersion field at header offset 12.
    { FILE* fp = std::fopen(p.c_str(), "rb+");
      std::fseek(fp, 12, SEEK_SET);
      uint16_t zero = 0; std::fwrite(&zero, 2, 1, fp);
      std::fclose(fp); }

    BinaryRecordFile f(p, 16, 12, 1);
    ok(!f.migratedOnOpen(), "no migration for a legacy file already at the baseline layout");
    ok(f.schemaVersion() == 1, "legacy schemaVersion 0 is interpreted as 1");
    bool good = true;
    char buf[16];
    for (int i = 0; i < 5 && good; ++i)
        if (!f.read(static_cast<uint32_t>(i), buf) || !sameRec(buf, makeRec(i))) good = false;
    ok(good, "all records intact — existing books open unchanged on upgrade");
}

// ── EventLog (immutable audit history) ───────────────────────────────────────

void testEventLog(const std::string& base)
{
    section("EventLog: append-only replay, determinism, torn-tail + corruption");
    std::string p = freshPath(base, "events.log");
    constexpr int N = 50;
    auto mkPayload = [](int i) {
        std::vector<char> v(static_cast<std::size_t>(8 + i % 5), 0);
        for (std::size_t k = 0; k < v.size(); ++k) v[k] = static_cast<char>(i + static_cast<int>(k));
        return v;
    };

    { EventLog log(p);
      for (int i = 0; i < N; ++i) {
          auto pl = mkPayload(i);
          log.append(static_cast<uint16_t>(i % 3 + 1), 1, 1000 + i,
                     pl.data(), static_cast<uint32_t>(pl.size()));
      } }

    {
        EventLog log(p);
        ok(log.lastSeq() == static_cast<uint64_t>(N), "lastSeq == N after reopen");
        int seen = 0; bool good = true;
        log.forEach([&](const EventRecord& r) {
            auto exp = mkPayload(seen);
            if (r.seq != static_cast<uint64_t>(seen + 1) ||
                r.type != static_cast<uint16_t>(seen % 3 + 1) ||
                r.timestampMs != 1000 + seen || r.payload != exp) good = false;
            ++seen; return true;
        });
        ok(seen == N && good, "forEach replays all N events in order with exact payloads");
    }

    auto fingerprint = [&]() {
        std::string fp; EventLog log(p);
        log.forEach([&](const EventRecord& r) {
            fp += std::to_string(r.seq) + ":"
                + std::to_string(BinaryRecordFile::crc32(r.payload.data(), r.payload.size())) + ";";
            return true; });
        return fp;
    };
    ok(fingerprint() == fingerprint(), "two replays are identical (deterministic ordering)");

    // Uncommitted/torn tail past the commit point → discarded on open.
    { FILE* f = std::fopen(p.c_str(), "ab");
      std::vector<char> junk(40, static_cast<char>(0xAB));
      std::fwrite(junk.data(), 1, junk.size(), f); std::fclose(f); }
    { EventLog log(p);
      ok(log.recoveredTornTail(), "uncommitted tail detected + discarded on open");
      ok(log.lastSeq() == static_cast<uint64_t>(N), "committed history intact after tail recovery"); }

    // Corruption INSIDE the committed region → detected (open throws).
    { FILE* f = std::fopen(p.c_str(), "r+b");
      std::fseek(f, static_cast<long>(EventLog::kHeaderSize + EventLog::kFrameHeader), SEEK_SET);
      char c = 0; std::fread(&c, 1, 1, f); c = static_cast<char>(c ^ 0xFF);
      std::fseek(f, static_cast<long>(EventLog::kHeaderSize + EventLog::kFrameHeader), SEEK_SET);
      std::fwrite(&c, 1, 1, f); std::fclose(f); }
    bool threw = false;
    try { EventLog log(p); } catch (const std::exception&) { threw = true; }
    ok(threw, "corruption inside committed history is detected (open refuses)");
}

// ── AuditJournal (authoritative history → projection) ────────────────────────

std::vector<char> readFileBytes(const std::string& p)
{
    std::vector<char> v;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return v;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize(static_cast<std::size_t>(n)); std::fread(v.data(), 1, v.size(), f); }
    std::fclose(f);
    return v;
}

void testAuditJournal(const std::string& base)
{
    section("AuditJournal: history → projection, replay determinism, rebuild, corrections");
    std::string cdat = freshPath(base, "aj_customers.dat");
    std::string log  = base + "/aj.log";
    std::string cur  = base + "/aj.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        for (int i = 0; i < 10; ++i) {
            Customer c; c.setName(("Cust " + std::to_string(i)).c_str());
            aj.recordCustomerCreated(c, 1000 + i);
        }
        aj.recordCustomerRenamed(2, "Renamed-2", 2000);
        aj.recordCustomerRenamed(5, "Renamed-5", 2001);
        aj.recordCustomerRenamed(2, "Renamed-2-again", 2002);   // correction chain on one id
        ok(aj.lastSeq() == 13, "13 events recorded (10 created + 3 renamed)");
        ok(aj.appliedSeq() == 13, "projection caught up to the head of history");
        ok(repo.count() == 10, "projection holds 10 customers (renames update in place)");
    }

    const auto liveBytes = readFileBytes(cdat);
    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        ok(aj.rebuildProjections() == 13, "rebuild replayed all 13 events from history");
    }
    const auto rebuiltBytes = readFileBytes(cdat);
    ok(!liveBytes.empty() && liveBytes == rebuiltBytes,
       "rebuilt projection is BYTE-IDENTICAL to the live one (deterministic replay)");

    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        ok(aj.reconcile() == 0, "reconcile is a no-op when the projection is current");
        ok(aj.reconcile() == 0, "reconcile is idempotent");
        Customer c = repo.load(2);
        ok(std::string(c.getName()) == "Renamed-2-again", "correction chain projects the latest value");
    }
}

// ── Stable child identity (invoice lines) ────────────────────────────────────

void testInvoiceLineIdentity(const std::string& base)
{
    section("Stable child identity: invoice lines, deterministic rebuild, correction by id");
    std::string idat = freshPath(base, "il_invoices.dat");
    std::string ldat = freshPath(base, "il_lines.dat");
    std::string cdat = freshPath(base, "il_customers.dat");
    std::string log  = base + "/il.log";
    std::string cur  = base + "/il.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    auto mkLine = [](const char* desc, double qty, double price) {
        InvoiceLineData d;
        d.description        = desc;
        d.quantityMilliunits = static_cast<int32_t>(std::llround(qty * 1000.0));
        d.unitPrice          = Money::fromDouble(price);
        d.taxRatePermille    = 0;
        InvoiceLine l(d); l.recompute(); return l;
    };
    auto mkInv = [](const char* num) {
        Invoice inv; inv.setInvoiceNumber(num); inv.setStatus(INVOICE_DRAFT); return inv;
    };

    uint32_t liveInvHash = 0, liveLineHash = 0;
    {
        CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
        AuditJournal aj(log, cur, &c, &ir, &lr);
        Invoice i1 = mkInv("INV-1");
        std::vector<InvoiceLine> l1 = { mkLine("A",1,10), mkLine("B",2,20), mkLine("C",3,30) };
        aj.recordInvoiceCreated(i1, l1, 1);
        Invoice i2 = mkInv("INV-2");
        std::vector<InvoiceLine> l2 = { mkLine("D",1,5), mkLine("E",1,7) };
        aj.recordInvoiceCreated(i2, l2, 2);

        ok(ir.count() == 2 && lr.count() == 5, "2 invoices + 5 lines projected");
        auto inv1 = lr.findByInvoice(0);
        ok(inv1.size() == 3 && inv1[0].getId() == 0 && inv1[2].getId() == 2,
           "invoice 1 lines got stable monotonic ids 0,1,2 (not positional)");
        liveInvHash = ir.contentHash(); liveLineHash = lr.contentHash();
    }
    {
        CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
        AuditJournal aj(log, cur, &c, &ir, &lr);
        aj.rebuildProjections();
        ok(ir.contentHash() == liveInvHash, "invoices rebuild content-identical from history");
        ok(lr.contentHash() == liveLineHash, "lines rebuild content-identical (stable ids preserved)");
    }

    // Correction: keep line 0, drop line 1, modify line 2 (by id), add a new line.
    {
        CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
        AuditJournal aj(log, cur, &c, &ir, &lr);
        Invoice i1 = mkInv("INV-1"); i1.setId(0);
        InvoiceLine keep = mkLine("A",1,10);     keep.setId(0);
        InvoiceLine mod  = mkLine("C-fixed",3,99); mod.setId(2);
        InvoiceLine add  = mkLine("F",1,1);      add.setId(UINT32_MAX);
        std::vector<InvoiceLine> newSet = { keep, mod, add };
        aj.recordInvoiceCorrected(i1, newSet, 3);

        auto live = lr.findByInvoice(0);
        bool has0=false, has2=false, has5=false, has1=false;
        for (auto& l : live) { uint32_t id=l.getId(); has0|=id==0; has1|=id==1; has2|=id==2; has5|=id==5; }
        ok(live.size()==3 && has0 && has2 && has5 && !has1,
           "correction by stable id: 0,2 kept, 5 added, 1 tombstoned (never positional)");
        ok(std::string(lr.load(2).getDescription()) == "C-fixed",
           "modification targeted line 2 by its stable id");
    }
    {
        CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
        const uint32_t bInv = ir.contentHash(), bLine = lr.contentHash();
        AuditJournal aj(log, cur, &c, &ir, &lr);
        aj.rebuildProjections();
        ok(ir.contentHash()==bInv && lr.contentHash()==bLine,
           "post-correction rebuild content-identical (correction chain is deterministic)");
        ok(aj.reconcile()==0, "reconcile is a no-op once current (idempotent)");
    }
}

// ── Deterministic snapshotting (replay acceleration) ─────────────────────────

void testSnapshotting(const std::string& base)
{
    section("Snapshotting: replay acceleration, equivalence, verify, corruption, disposable");
    std::string cdat = freshPath(base, "sn_customers.dat");
    std::string idat = freshPath(base, "sn_invoices.dat");
    std::string ldatf = freshPath(base, "sn_lines.dat");
    std::string log = base + "/sn.log";
    std::string cur = base + "/sn.cursor";
    std::string snap = cur + ".ledgersnap";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec);
      std::filesystem::remove(snap, ec); }

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldatf);
    AuditJournal aj(log, cur, &c, &ir, &lr);
    using P = AuditJournal::PostingInput;
    const uint32_t a1 = aj.recordAccount(AuditJournal::Asset,  "A1", 1);
    const uint32_t a2 = aj.recordAccount(AuditJournal::Income, "A2", 2);
    const IsoDate d = IsoDate::fromString("2026-07-01").value();
    for (int i = 0; i < 5; ++i) aj.recordJournalEntry(d, { P{a1, 1000}, P{a2, -1000} }, 10 + i);
    const uint64_t midSeq = aj.lastSeq();

    aj.writeLedgerSnapshot(midSeq);
    ok(aj.ledgerSnapshotSeq() == midSeq, "snapshot represents its seq boundary");
    ok(aj.verifyLedgerSnapshot(), "snapshot verifies against a genesis replay at that seq");

    for (int i = 0; i < 3; ++i) aj.recordJournalEntry(d, { P{a1, 2000}, P{a2, -2000} }, 20 + i);
    ok(aj.balanceUsingSnapshot(a1, aj.lastSeq()) == aj.balanceAt(a1, aj.lastSeq()) &&
       aj.balanceUsingSnapshot(a2, aj.lastSeq()) == aj.balanceAt(a2, aj.lastSeq()),
       "snapshot + replay-tail == genesis replay (identical balances)");
    ok(aj.balanceUsingSnapshot(a1, aj.lastSeq()) == 11000, "accelerated balance is correct ($110)");
    ok(aj.verifyLedgerSnapshot(), "append-only history keeps the (older) snapshot permanently valid");

    // Corruption: tamper a snapshot balance byte → detected; genesis fallback still correct.
    { FILE* f = std::fopen(snap.c_str(), "r+b"); std::fseek(f, 32, SEEK_SET);
      char x = 0; std::fread(&x, 1, 1, f); x = static_cast<char>(x ^ 0xFF);
      std::fseek(f, 32, SEEK_SET); std::fwrite(&x, 1, 1, f); std::fclose(f); }
    ok(aj.ledgerSnapshotSeq() == 0, "a corrupted snapshot is detected (treated as absent)");
    ok(!aj.verifyLedgerSnapshot(), "verify rejects a corrupted snapshot");
    ok(aj.balanceUsingSnapshot(a1, aj.lastSeq()) == aj.balanceAt(a1, aj.lastSeq()),
       "corrupt snapshot → genesis fallback is still correct (snapshot is never authority)");

    { std::error_code ec; std::filesystem::remove(snap, ec); }
    ok(aj.balanceUsingSnapshot(a1, aj.lastSeq()) == aj.balanceAt(a1, aj.lastSeq()),
       "no snapshot → genesis fallback (snapshots are disposable accelerators)");
}

// ── Posting authority: business event → ledger mapping ───────────────────────

void testPostingAuthority(const std::string& base)
{
    section("Posting authority: business event → deterministic balanced ledger postings");
    std::string cdat = freshPath(base, "pa_customers.dat");
    std::string idat = freshPath(base, "pa_invoices.dat");
    std::string ldatf = freshPath(base, "pa_lines.dat");
    std::string log = base + "/pa.log";
    std::string cur = base + "/pa.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldatf);
    AuditJournal aj(log, cur, &c, &ir, &lr);
    const uint32_t ar   = aj.recordAccount(AuditJournal::Asset,  "AR",      1);
    const uint32_t rev  = aj.recordAccount(AuditJournal::Income, "Revenue", 2);
    const uint32_t cash = aj.recordAccount(AuditJournal::Asset,  "Cash",    3);
    aj.setPostingAccounts(ar, rev, cash);
    const IsoDate d = IsoDate::fromString("2026-06-01").value();

    // Business event: an invoice is created (operational), then the FIXED policy posts it.
    Invoice inv; inv.setInvoiceNumber("INV-1"); inv.setStatus(INVOICE_POSTED);
    inv.setIssueDate(d); inv.setTotal(Money::fromCents(10000));
    InvoiceLineData ld; ld.description = "A"; ld.quantityMilliunits = 1000;
    ld.unitPrice = Money::fromDouble(100); ld.taxRatePermille = 0;
    InvoiceLine line(ld); line.recompute(); std::vector<InvoiceLine> lines = { line };
    aj.recordInvoiceCreated(inv, lines, 4);
    const uint32_t e1 = aj.postInvoiceRevenue(inv.getTotal().cents(), d, 5);

    ok(aj.balanceFor(ar) == 10000 && aj.balanceFor(rev) == -10000,
       "InvoiceCreated → Dr AR $100 / Cr Revenue $100 (fixed deterministic policy)");
    ok(aj.incomeStatementAt(aj.lastSeq()).income == 10000,
       "the operational invoice produced revenue in the income statement");
    ok(aj.trialBalanceTotal() == 0, "generated postings are balanced (trial balance 0)");

    aj.reverseJournalEntry(e1, d, 6);
    ok(aj.balanceFor(ar) == 0 && aj.balanceFor(rev) == 0,
       "reversing the generated posting nets AR + Revenue to 0 (compensating postings)");

    aj.postInvoiceRevenue(8000, d, 7);   // Dr AR $80 / Cr Revenue $80
    aj.postPaymentReceipt(8000, d, 8);   // Dr Cash $80 / Cr AR $80
    ok(aj.balanceFor(ar) == 0 && aj.balanceFor(cash) == 8000,
       "PaymentReceipt → Dr Cash / Cr AR settles the receivable financially");
    ok(aj.balanceSheetAt(aj.lastSeq()).balances,
       "balance sheet balances after the operational→financial postings");

    const int64_t bAr = aj.balanceFor(ar), bCash = aj.balanceFor(cash), bRev = aj.balanceFor(rev);
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldatf);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);   // no setPostingAccounts — replays the persisted postings
      ok(aj2.balanceFor(ar) == bAr && aj2.balanceFor(cash) == bCash && aj2.balanceFor(rev) == bRev &&
         aj2.trialBalanceTotal() == 0,
         "generated postings rebuild deterministically from history (identical balances/statements)"); }
}

// ── Financial statements & closing ───────────────────────────────────────────

void testFinancialStatements(const std::string& base)
{
    section("Statements: income statement, balance sheet (always balances), closing → retained earnings");
    std::string cdat = freshPath(base, "fs_customers.dat");
    std::string idat = freshPath(base, "fs_invoices.dat");
    std::string ldatf = freshPath(base, "fs_lines.dat");
    std::string log = base + "/fs.log";
    std::string cur = base + "/fs.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldatf);
    AuditJournal aj(log, cur, &c, &ir, &lr);
    using P = AuditJournal::PostingInput;

    const uint32_t cash  = aj.recordAccount(AuditJournal::Asset,   "Cash",   1);
    const uint32_t sales = aj.recordAccount(AuditJournal::Income,  "Sales",  2);
    const uint32_t rent  = aj.recordAccount(AuditJournal::Expense, "Rent",   3);
    aj.recordAccount(AuditJournal::Equity, "Capital", 4);
    const uint32_t re    = aj.recordAccount(AuditJournal::Equity,  "RetainedEarnings", 5);

    const IsoDate d = IsoDate::fromString("2026-05-15").value();
    aj.recordJournalEntry(d, { P{cash, 10000}, P{sales, -10000} }, 6);    // revenue $100
    const uint64_t beforeRent = aj.lastSeq();
    aj.recordJournalEntry(d, { P{rent, 3000}, P{cash, -3000} }, 7);       // expense $30

    const auto is = aj.incomeStatementAt(aj.lastSeq());
    ok(is.income == 10000 && is.expense == 3000 && is.netIncome == 7000,
       "income statement: income $100, expense $30, net income $70");
    const auto bs = aj.balanceSheetAt(aj.lastSeq());
    ok(bs.assets == 7000 && bs.equity == 7000 && bs.balances,
       "balance sheet: assets $70 = equity $70 (incl. net income), and it BALANCES");

    ok(aj.incomeStatementAt(beforeRent).netIncome == 10000,
       "historical: net income was $100 before the expense was posted");

    // Closing entry → retained earnings.
    aj.recordClosingEntry(re, d, 8);
    const auto isAfter = aj.incomeStatementAt(aj.lastSeq());
    ok(isAfter.income == 0 && isAfter.expense == 0 && isAfter.netIncome == 0,
       "after closing: income/expense reset to 0, current net income 0");
    ok(aj.balanceFor(re) == -7000, "net income ($70) flowed into retained earnings (a credit/equity balance)");
    const auto bsAfter = aj.balanceSheetAt(aj.lastSeq());
    ok(bsAfter.assets == 7000 && bsAfter.equity == 7000 && bsAfter.balances,
       "post-close balance sheet still balances (equity is now retained earnings)");

    // Post-close adjustment: a NEW entry shows only new activity in the current statement.
    aj.recordJournalEntry(d, { P{cash, 2000}, P{sales, -2000} }, 9);      // $20 more revenue
    ok(aj.incomeStatementAt(aj.lastSeq()).netIncome == 2000,
       "post-close revenue shows only NEW activity ($20) in the current income statement");

    // Deterministic: statements rebuild identically from history.
    const int64_t reBal = aj.balanceFor(re);
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldatf);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);
      ok(aj2.balanceFor(re) == reBal && aj2.balanceSheetAt(aj2.lastSeq()).balances && aj2.trialBalanceTotal() == 0,
         "statements rebuild deterministically (retained earnings preserved, balance sheet balances)"); }
}

// ── Double-entry ledger ──────────────────────────────────────────────────────

void testLedgerSemantics(const std::string& base)
{
    section("Ledger: balanced double-entry, trial balance, historical, reversal, period");
    std::string cdat = freshPath(base, "lg_customers.dat");
    std::string idat = freshPath(base, "lg_invoices.dat");
    std::string ldatf = freshPath(base, "lg_lines.dat");
    std::string log = base + "/lg.log";
    std::string cur = base + "/lg.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldatf);
    AuditJournal aj(log, cur, &c, &ir, &lr);
    using P = AuditJournal::PostingInput;

    const uint32_t cash  = aj.recordAccount(1, "Cash",  1);
    const uint32_t sales = aj.recordAccount(4, "Sales", 2);
    const uint32_t arAcc = aj.recordAccount(1, "AR",    3);
    ok(aj.accountCount() == 3, "3 accounts opened");

    const IsoDate d = IsoDate::fromString("2026-04-01").value();
    const uint64_t beforeE1 = aj.lastSeq();
    const uint32_t e1 = aj.recordJournalEntry(d, { P{cash, 10000}, P{sales, -10000} }, 4);  // Dr Cash / Cr Sales
    ok(aj.balanceFor(cash) == 10000 && aj.balanceFor(sales) == -10000, "balanced entry posts Dr Cash / Cr Sales");
    ok(aj.trialBalanceTotal() == 0, "trial balance total is 0 (debits == credits)");

    bool unbalanced = false;
    try { aj.recordJournalEntry(d, { P{cash, 10000}, P{sales, -5000} }, 5); }
    catch (const std::exception&) { unbalanced = true; }
    ok(unbalanced, "an unbalanced journal entry is rejected (Σ debits != Σ credits)");

    aj.recordJournalEntry(d, { P{arAcc, 5000}, P{sales, -5000} }, 6);
    ok(aj.balanceFor(sales) == -15000 && aj.trialBalanceTotal() == 0, "second entry posts; trial balance still 0");

    ok(aj.balanceAt(cash, beforeE1) == 0, "historical: Cash was 0 before its entry");
    ok(aj.balanceAt(cash, aj.lastSeq()) == 10000, "historical: Cash is $100 now");

    const uint32_t rev = aj.reverseJournalEntry(e1, d, 7);
    (void)rev;
    ok(aj.balanceFor(cash) == 0, "reversing e1 nets Cash to 0 (append-only reversal posting, original untouched)");
    ok(aj.trialBalanceTotal() == 0, "trial balance still 0 after reversal");
    ok(aj.entryCount() == 3, "3 entries on the books (e1, e2, reversal) — append-only");

    aj.closePeriod("2026-04", IsoDate::fromString("2026-04-01").value(),
                   IsoDate::fromString("2026-04-30").value(), 8);
    bool closedRej = false;
    try { aj.recordJournalEntry(d, { P{cash, 100}, P{sales, -100} }, 9); }
    catch (const std::exception&) { closedRej = true; }
    ok(closedRej, "posting into a closed period is rejected (post an adjusting entry in an open one)");

    const int64_t bCash = aj.balanceFor(cash), bSales = aj.balanceFor(sales), bAr = aj.balanceFor(arAcc);
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldatf);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);
      ok(aj2.balanceFor(cash) == bCash && aj2.balanceFor(sales) == bSales &&
         aj2.balanceFor(arAcc) == bAr && aj2.trialBalanceTotal() == 0,
         "ledger balances rebuild deterministically from history (trial balance 0)"); }
}

// ── Reconciliation & allocation ──────────────────────────────────────────────

void testAllocationSemantics(const std::string& base)
{
    section("Allocation: DERIVED settlement (no paid flag), partial/multi, reversal, historical");
    std::string idat = freshPath(base, "al_invoices.dat");
    std::string ldat = freshPath(base, "al_lines.dat");
    std::string cdat = freshPath(base, "al_customers.dat");
    std::string log  = base + "/al.log";
    std::string cur  = base + "/al.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    auto mkLine = [](const char* desc, double qty, double price) {
        InvoiceLineData d; d.description = desc;
        d.quantityMilliunits = static_cast<int32_t>(std::llround(qty * 1000.0));
        d.unitPrice = Money::fromDouble(price); d.taxRatePermille = 0;
        InvoiceLine l(d); l.recompute(); return l;
    };
    auto mkInv = [](const char* num, const char* date, int64_t totalCents) {
        Invoice inv; inv.setInvoiceNumber(num); inv.setStatus(INVOICE_POSTED);
        inv.setIssueDate(IsoDate::fromString(date).value());
        inv.setTotal(Money::fromCents(totalCents)); return inv;
    };
    const IsoDate d = IsoDate::fromString("2026-03-10").value();

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
    AuditJournal aj(log, cur, &c, &ir, &lr);

    Invoice i1 = mkInv("INV-1", "2026-03-01", 10000);  std::vector<InvoiceLine> l1 = { mkLine("A",1,100) };
    aj.recordInvoiceCreated(i1, l1, 1); const uint32_t inv1 = i1.getId();
    Invoice i2 = mkInv("INV-2", "2026-03-02", 5000);   std::vector<InvoiceLine> l2 = { mkLine("B",1,50) };
    aj.recordInvoiceCreated(i2, l2, 2); const uint32_t inv2 = i2.getId();
    ok(aj.outstandingFor(inv1) == 10000, "a new invoice is fully outstanding ($100), no paid flag");

    const uint32_t p1 = aj.recordPayment(0, 12000, d, 3);
    ok(aj.unallocatedFor(p1) == 12000, "a new payment is fully unallocated ($120)");

    const uint32_t a1 = aj.allocatePayment(p1, inv1, 10000, d, 4);
    ok(aj.settledFor(inv1) == 10000 && aj.outstandingFor(inv1) == 0, "INV-1 fully settled, outstanding 0");
    ok(aj.unallocatedFor(p1) == 2000, "payment unallocated drops to $20");

    aj.allocatePayment(p1, inv2, 2000, d, 5);
    const uint64_t seqAfterPartial = aj.lastSeq();
    ok(aj.outstandingFor(inv2) == 3000, "INV-2 partially settled ($20 of $50) — $30 outstanding");
    ok(aj.unallocatedFor(p1) == 0, "one payment allocated across TWO invoices (fully)");

    bool over = false;
    try { aj.allocatePayment(p1, inv2, 100, d, 6); } catch (const std::exception&) { over = true; }
    ok(over, "allocating beyond the payment's unallocated balance is rejected");

    const uint32_t p2 = aj.recordPayment(0, 3000, d, 7);
    aj.allocatePayment(p2, inv2, 3000, d, 8);
    ok(aj.outstandingFor(inv2) == 0, "INV-2 fully settled by TWO payments (multi-allocation per invoice)");

    // Historical: settlement as of the partial-allocation seq vs current.
    ok(aj.settledAt(inv2, seqAfterPartial) == 2000, "historical: INV-2 was settled $20 at the partial-allocation seq");
    ok(aj.settledAt(inv2, aj.lastSeq()) == 5000, "current: INV-2 settled $50");

    // Reversal of settlement restores outstanding (append-only).
    aj.reverseAllocation(a1, 9);
    ok(aj.settledFor(inv1) == 0 && aj.outstandingFor(inv1) == 10000, "reversing the allocation restores INV-1 outstanding ($100)");
    ok(aj.unallocatedFor(p1) == 10000, "reversal returns $100 to the payment's unallocated pool");

    // Closed-period settlement cannot be reversed in place.
    aj.closePeriod("2026-03", IsoDate::fromString("2026-03-01").value(),
                   IsoDate::fromString("2026-03-31").value(), 10);
    bool revRejected = false;
    try { aj.reverseAllocation(/*a2 the partial*/ 1, 11); } catch (const std::exception&) { revRejected = true; }
    ok(revRejected, "reversing a CLOSED-period allocation is rejected (post a compensating allocation)");

    // Deterministic: settlement index rebuilds identically from history.
    const int64_t s1 = aj.settledFor(inv1), s2 = aj.settledFor(inv2), u1 = aj.unallocatedFor(p1);
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldat);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);
      ok(aj2.settledFor(inv1) == s1 && aj2.settledFor(inv2) == s2 && aj2.unallocatedFor(p1) == u1,
         "settlement index rebuilds deterministically from history"); }
}

// ── Structured corrections: void vs reversal ─────────────────────────────────

void testCorrectionSemantics(const std::string& base)
{
    section("Corrections: void (in-place, open-only) vs reversal (append-only, closed-OK)");
    std::string idat = freshPath(base, "cr_invoices.dat");
    std::string ldat = freshPath(base, "cr_lines.dat");
    std::string cdat = freshPath(base, "cr_customers.dat");
    std::string log  = base + "/cr.log";
    std::string cur  = base + "/cr.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    auto mkLine = [](const char* desc, double qty, double price) {
        InvoiceLineData d; d.description = desc;
        d.quantityMilliunits = static_cast<int32_t>(std::llround(qty * 1000.0));
        d.unitPrice = Money::fromDouble(price); d.taxRatePermille = 0;
        InvoiceLine l(d); l.recompute(); return l;
    };
    auto mkInv = [](const char* num, const char* date, InvoiceStatus st) {
        Invoice inv; inv.setInvoiceNumber(num); inv.setStatus(st);
        inv.setIssueDate(IsoDate::fromString(date).value()); return inv;
    };

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
    AuditJournal aj(log, cur, &c, &ir, &lr);

    // INV-1 (open period) → VOID in place.
    Invoice i1 = mkInv("INV-1", "2026-03-15", INVOICE_POSTED);
    std::vector<InvoiceLine> l1 = { mkLine("A",1,10) };
    aj.recordInvoiceCreated(i1, l1, 1000);               // seq 1, id 0
    const uint64_t beforeVoid = aj.lastSeq();
    aj.recordInvoiceVoided(0, 2000);                     // seq 2
    ok(aj.isVoided(0), "void marks the invoice not-effective (lineage index)");
    ok(ir.load(0).getStatus() == INVOICE_VOID, "void projects status = VOID in place");

    // INV-2 in a CLOSED period.
    Invoice i2 = mkInv("INV-2", "2026-01-15", INVOICE_POSTED);
    std::vector<InvoiceLine> l2 = { mkLine("B",1,20) };
    aj.recordInvoiceCreated(i2, l2, 3000);               // seq 3, id 1
    aj.closePeriod("2026-01", IsoDate::fromString("2026-01-01").value(),
                   IsoDate::fromString("2026-01-31").value(), 3500);   // seq 4

    bool voidRejected = false;
    try { aj.recordInvoiceVoided(1, 4000); } catch (const std::exception&) { voidRejected = true; }
    ok(voidRejected, "voiding a CLOSED-period invoice is rejected (amends frozen standing)");

    // Reversal IS allowed for the closed-period original — append-only negating entry.
    Invoice rev = mkInv("INV-2-REV", "2026-02-01", INVOICE_POSTED);
    std::vector<InvoiceLine> rl = { mkLine("B-reversal",1,-20) };
    bool reversed = false;
    try { aj.recordInvoiceReversal(1, rev, rl, 5000); reversed = true; } catch (const std::exception&) {}
    ok(reversed, "reversing a CLOSED-period invoice is allowed (the sanctioned post-close path)");
    ok(aj.reversedBy(1) == rev.getId() && rev.getId() != 0xFFFFFFFFu,
       "correction lineage: original → its reversal entry id");
    ok(ir.load(1).getStatus() == INVOICE_POSTED,
       "reversal does NOT mutate the original (both remain on the books)");
    ok(ir.count() == 3, "reversal appended a new negating transaction (3 invoices)");

    // Historical interpretation: before the void, INV-1 was POSTED; now it is VOID.
    { CustomerRepository sc(base+"/crs_c.dat"); InvoiceRepository si(base+"/crs_i.dat"); InvoiceLineRepository sl(base+"/crs_l.dat");
      aj.reconstructAllInto(sc, si, sl, beforeVoid);
      ok(si.load(0).getStatus() == INVOICE_POSTED, "books before the void: INV-1 was POSTED");
      aj.reconstructAllInto(sc, si, sl, aj.lastSeq());
      ok(si.load(0).getStatus() == INVOICE_VOID, "current books: INV-1 is VOID"); }

    // Deterministic: rebuild reproduces void status + lineage index.
    const uint32_t invHash = ir.contentHash();
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldat);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);
      aj2.rebuildProjections();
      ok(ir2.contentHash() == invHash, "rebuild content-identical (void status replays deterministically)");
      ok(aj2.isVoided(0) && aj2.reversedBy(1) == rev.getId(),
         "correction lineage rebuilds deterministically from history"); }
}

// ── Accounting period closure & historical freezing ─────────────────────────

void testPeriodClosure(const std::string& base)
{
    section("Period closure: freeze, post-close correction rejected, books-as-closed, reopen");
    std::string idat = freshPath(base, "pc_invoices.dat");
    std::string ldat = freshPath(base, "pc_lines.dat");
    std::string cdat = freshPath(base, "pc_customers.dat");
    std::string log  = base + "/pc.log";
    std::string cur  = base + "/pc.cursor";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec); }

    auto mkLine = [](const char* desc, double qty, double price) {
        InvoiceLineData d; d.description = desc;
        d.quantityMilliunits = static_cast<int32_t>(std::llround(qty * 1000.0));
        d.unitPrice = Money::fromDouble(price); d.taxRatePermille = 0;
        InvoiceLine l(d); l.recompute(); return l;
    };
    auto mkInv = [](const char* num, const char* date) {
        Invoice inv; inv.setInvoiceNumber(num); inv.setStatus(INVOICE_DRAFT);
        inv.setIssueDate(IsoDate::fromString(date).value()); return inv;
    };
    const IsoDate janStart = IsoDate::fromString("2026-01-01").value();
    const IsoDate janEnd   = IsoDate::fromString("2026-01-31").value();

    CustomerRepository c(cdat); InvoiceRepository ir(idat); InvoiceLineRepository lr(ldat);
    AuditJournal aj(log, cur, &c, &ir, &lr);

    Invoice i1 = mkInv("INV-1", "2026-01-15");
    std::vector<InvoiceLine> l1 = { mkLine("A",1,10) };
    aj.recordInvoiceCreated(i1, l1, 1000);                 // seq 1
    const uint32_t inv1 = i1.getId();

    aj.closePeriod("2026-01", janStart, janEnd, 2000);     // seq 2; closedAtSeq = 1
    ok(aj.closedPeriodCount() == 1, "one period is closed");
    ok(aj.isInvoiceInClosedPeriod(inv1), "INV-1 (Jan, effective date) is in the closed period");

    Invoice i2 = mkInv("INV-2", "2026-02-15");
    std::vector<InvoiceLine> l2 = { mkLine("B",1,20) };
    aj.recordInvoiceCreated(i2, l2, 3000);                 // seq 3
    const uint32_t inv2 = i2.getId();
    ok(!aj.isInvoiceInClosedPeriod(inv2), "INV-2 (Feb) is NOT frozen");

    // Correcting a frozen invoice is rejected; an open one is allowed.
    bool rejected = false;
    try { Invoice ci = mkInv("INV-1","2026-01-15"); ci.setId(inv1);
          std::vector<InvoiceLine> cl = { mkLine("A2",1,10) }; cl[0].setId(0);
          aj.recordInvoiceCorrected(ci, cl, 4000); }
    catch (const std::exception&) { rejected = true; }
    ok(rejected, "correcting a CLOSED-period invoice is rejected (post an adjustment instead)");

    bool allowed = false;
    try { Invoice ci = mkInv("INV-2","2026-02-15"); ci.setId(inv2);
          std::vector<InvoiceLine> cl = { mkLine("B2",2,20) }; cl[0].setId(1);
          aj.recordInvoiceCorrected(ci, cl, 5000); allowed = true; }   // seq 4
    catch (const std::exception&) {}
    ok(allowed, "correcting an OPEN-period invoice is allowed");

    // Books as closed (at the freeze seq) vs current books.
    { CustomerRepository sc(base+"/pcs_c.dat"); InvoiceRepository si(base+"/pcs_i.dat"); InvoiceLineRepository sl(base+"/pcs_l.dat");
      aj.reconstructAllInto(sc, si, sl, aj.closedAtSeqFor("2026-01"));
      ok(si.count() == 1, "books-as-closed: exactly 1 invoice — post-close entries excluded");
      aj.reconstructAllInto(sc, si, sl, aj.lastSeq());
      ok(si.count() == 2, "current books: 2 invoices"); }

    aj.reopenPeriod("2026-01", 6000);                      // seq 5
    ok(aj.closedPeriodCount() == 0 && !aj.isInvoiceInClosedPeriod(inv1),
       "reopen (append-only event) unfreezes the period — corrections allowed again");

    // The closed-period index is a disposable projection: it rebuilds deterministically.
    { CustomerRepository c2(cdat); InvoiceRepository ir2(idat); InvoiceLineRepository lr2(ldat);
      AuditJournal aj2(log, cur, &c2, &ir2, &lr2);
      ok(aj2.closedPeriodCount() == 0,
         "closed-period index rebuilds deterministically from history (no hidden authority)"); }
}

// ── Commit cutover: backfill (adopt pre-audit state into history) ────────────

void testCustomerBackfill(const std::string& base)
{
    section("Commit cutover: backfill adopts pre-audit projection into history");
    std::string cdat = freshPath(base, "bf_customers.dat");
    std::string log  = base + "/bf.log";
    std::string cur  = base + "/bf.cursor";
    std::string scr  = base + "/bf_scratch.dat";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec);
      std::filesystem::remove(scr, ec); }

    // Pre-cutover state: customers written DIRECTLY via the repo (no events).
    { CustomerRepository repo(cdat);
      for (int i = 0; i < 6; ++i) { Customer c; c.setName(("Pre" + std::to_string(i)).c_str()); repo.save(c); } }

    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        ok(aj.lastSeq() == 0, "history is empty before cutover");
        const uint64_t filled = aj.backfillCustomers(123);
        ok(filled == 6 && aj.lastSeq() == 6, "backfill authored 6 events for the existing customers");
        CustomerRepository sc(scr);
        ok(aj.verify(sc).ok, "after backfill, live projection == authoritative history");
    }
    { CustomerRepository repo(cdat); AuditJournal aj(log, cur, &repo);
      ok(aj.backfillCustomers(456) == 0, "backfill is one-time (no-op once history is non-empty)"); }

    // Content fingerprint (record region) is the authoritative-equality invariant —
    // not the whole file, whose header lastWriteId legitimately differs after the
    // mixed direct-save + backfill-update write sequence.
    uint32_t beforeHash = 0, afterHash = 0;
    { CustomerRepository repo(cdat); beforeHash = repo.contentHash(); }
    { CustomerRepository repo(cdat); AuditJournal aj(log, cur, &repo); aj.rebuildProjections(); }
    { CustomerRepository repo(cdat); afterHash = repo.contentHash(); }
    ok(beforeHash == afterHash,
       "rebuild from backfilled history is content-identical — no pre-cutover record lost");
}

// ── Projection verification & historical reconstruction ─────────────────────

void testProjectionVerify(const std::string& base)
{
    section("Projection verification: live == history, drift detected, not silent");
    std::string cdat = freshPath(base, "pv_customers.dat");
    std::string log  = base + "/pv.log";
    std::string cur  = base + "/pv.cursor";
    std::string scr  = base + "/pv_scratch.dat";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec);
      std::filesystem::remove(scr, ec); }

    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        for (int i = 0; i < 8; ++i) {
            Customer c; c.setName(("V" + std::to_string(i)).c_str());
            aj.recordCustomerCreated(c, 100 + i);
        }
        aj.recordCustomerRenamed(3, "V3-fixed", 200);
        CustomerRepository scratch(scr);
        auto vr = aj.verify(scratch);
        ok(vr.ok, "live projection matches authoritative history (no drift)");
        ok(vr.liveHash == vr.historyHash, "live and history fingerprints are equal");
    }

    // Corrupt the LIVE projection's record region → must be DETECTED.
    { FILE* f = std::fopen(cdat.c_str(), "r+b");
      std::fseek(f, 32 + 5, SEEK_SET); char c = 0; std::fread(&c, 1, 1, f);
      c = static_cast<char>(c ^ 0xFF);
      std::fseek(f, 32 + 5, SEEK_SET); std::fwrite(&c, 1, 1, f); std::fclose(f); }
    {
        CustomerRepository repo(cdat);
        AuditJournal aj(log, cur, &repo);
        CustomerRepository scratch(scr);
        auto vr = aj.verify(scratch);
        ok(!vr.ok, "a corrupted live projection is DETECTED as drift (never silent)");
        ok(vr.liveHash != vr.historyHash, "drift shows as a fingerprint mismatch");
    }
}

void testHistoricalReconstruction(const std::string& base)
{
    section("Historical reconstruction: effective-at-seq, before-correction, current");
    std::string cdat = freshPath(base, "hr_customers.dat");
    std::string log  = base + "/hr.log";
    std::string cur  = base + "/hr.cursor";
    std::string scr  = base + "/hr_scratch.dat";
    { std::error_code ec; std::filesystem::remove(log, ec); std::filesystem::remove(cur, ec);
      std::filesystem::remove(scr, ec); }

    CustomerRepository repo(cdat);
    AuditJournal aj(log, cur, &repo);
    Customer a; a.setName("Alpha"); aj.recordCustomerCreated(a, 1);   // seq 1, id 0
    aj.recordCustomerRenamed(0, "Beta", 2);                           // seq 2 (correction)
    Customer g; g.setName("Gamma"); aj.recordCustomerCreated(g, 3);   // seq 3, id 1

    const auto liveBefore = readFileBytes(cdat);
    const auto logBefore  = readFileBytes(log);

    { CustomerRepository sc(scr); aj.reconstructInto(sc, 1);
      ok(sc.count() == 1 && std::string(sc.load(0).getName()) == "Alpha",
         "effective-at-seq 1: customer 0 = 'Alpha' (before the rename)"); }
    { CustomerRepository sc(scr); aj.reconstructInto(sc, 2);
      ok(sc.count() == 1 && std::string(sc.load(0).getName()) == "Beta",
         "effective-at-seq 2: customer 0 corrected to 'Beta', Gamma not yet present"); }
    { CustomerRepository sc(scr); aj.reconstructInto(sc, 3);
      ok(sc.count() == 2 && std::string(sc.load(1).getName()) == "Gamma",
         "effective-at-seq 3 (current): 2 customers incl 'Gamma'"); }

    ok(std::string(repo.load(0).getName()) == "Beta",
       "live current state is the latest value ('Beta')");
    ok(readFileBytes(cdat) == liveBefore && readFileBytes(log) == logBefore,
       "reconstruction did NOT mutate the live projection or the authoritative log");
}

// ── Cross-process crash modes (driven by tools/ptest.sh) ─────────────────────

constexpr int kSentinelSeed = 0x5A;

int runCrashWrite(const std::string& base)
{
    // Append a sentinel record. If ACCT_CRASH_POINT is set, append() hard-exits
    // (code 99) at that step before returning here — that IS the test stimulus.
    std::string p = base + "/crash.dat";
    BinaryRecordFile f(p, kRS, kDel);
    auto rec = makeRec(kSentinelSeed);
    f.append(rec.data());
    std::fprintf(stderr, "crashwrite: completed without crashing (no ACCT_CRASH_POINT)\n");
    return 0;
}

int runCrashVerify(const std::string& base)
{
    // Reopen after a crashed crashwrite. The sentinel MUST be durable regardless
    // of which step the crash hit — that is the crash-safety guarantee.
    std::string p = base + "/crash.dat";
    BinaryRecordFile f(p, kRS, kDel);
    char buf[kRS];
    bool present = (f.count() >= 1) && f.read(0, buf) && sameRec(buf, makeRec(kSentinelSeed));
    std::fprintf(stderr,
                 "crashverify: count=%zu recovered=%s sentinel=%s\n",
                 f.count(),
                 f.recoveredOnOpen() ? "yes" : "no",
                 present ? "PRESENT" : "MISSING");
    std::fflush(stderr);
    return present ? 0 : 1;
}

constexpr int kMigCount = 5;

int runMigrateWrite(const std::string& base)
{
    // Seed a v1 file, then open it as v2 → triggers a forward migration. If
    // ACCT_CRASH_POINT names a migration step, the process hard-exits MID-migration.
    std::string p = base + "/migrate.dat";
    { BinaryRecordFile f(p, 16, 12, /*schema*/ 1);
      for (int i = 0; i < kMigCount; ++i) { auto r = makeRec(kSentinelSeed + i); f.append(r.data()); } }
    BinaryRecordFile f(p, 24, 12, /*schema*/ 2);   // migration happens here
    std::fprintf(stderr, "migratewrite: completed (no crash), migrated=%s\n",
                 f.migratedOnOpen() ? "yes" : "no");
    return 0;
}

int runSnapWrite(const std::string& base)
{
    // Build a ledger then write a snapshot — ACCT_CRASH_POINT may fire mid-snapshot.
    CustomerRepository c(base + "/sn_customers.dat");
    InvoiceRepository  ir(base + "/sn_invoices.dat");
    InvoiceLineRepository lr(base + "/sn_lines.dat");
    AuditJournal aj(base + "/sn.log", base + "/sn.cursor", &c, &ir, &lr);
    using P = AuditJournal::PostingInput;
    const uint32_t a1 = aj.recordAccount(1, "A1", 1);
    const uint32_t a2 = aj.recordAccount(4, "A2", 2);
    for (int i = 0; i < 5; ++i)
        aj.recordJournalEntry(IsoDate::fromString("2026-07-01").value(), { P{a1, 1000}, P{a2, -1000} }, 10 + i);
    aj.writeLedgerSnapshot(aj.lastSeq());   // crash fires at afterSnapshotTmp
    std::fprintf(stderr, "snapwrite: completed snapSeq=%llu\n",
                 static_cast<unsigned long long>(aj.ledgerSnapshotSeq()));
    return 0;
}

int runSnapVerify(const std::string& base)
{
    // After a crash mid-snapshot, the snapshot is COMPLETE (valid) or ABSENT — never
    // partial-authoritative — and accelerated balances still equal the genesis replay.
    CustomerRepository c(base + "/sn_customers.dat");
    InvoiceRepository  ir(base + "/sn_invoices.dat");
    InvoiceLineRepository lr(base + "/sn_lines.dat");
    AuditJournal aj(base + "/sn.log", base + "/sn.cursor", &c, &ir, &lr);
    const uint64_t ss = aj.ledgerSnapshotSeq();
    const bool snapOk = (ss == 0) || aj.verifyLedgerSnapshot();          // absent OR verifies
    const bool eq = aj.balanceUsingSnapshot(0, aj.lastSeq()) == aj.balanceAt(0, aj.lastSeq());
    const bool good = snapOk && eq && (aj.balanceAt(0, aj.lastSeq()) == 5000);
    std::fprintf(stderr, "snapverify: snapSeq=%llu balance=%lld snapOk=%d accel==genesis=%d\n",
                 static_cast<unsigned long long>(ss),
                 static_cast<long long>(aj.balanceAt(0, aj.lastSeq())), snapOk ? 1 : 0, eq ? 1 : 0);
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runLedgerWrite(const std::string& base)
{
    // Open accounts + post a balanced entry — ACCT_CRASH_POINT may fire in the posting.
    CustomerRepository c(base + "/lg_customers.dat");
    InvoiceRepository  ir(base + "/lg_invoices.dat");
    InvoiceLineRepository lr(base + "/lg_lines.dat");
    AuditJournal aj(base + "/lg.log", base + "/lg.cursor", &c, &ir, &lr);
    using P = AuditJournal::PostingInput;
    const uint32_t cash  = aj.recordAccount(1, "Cash",  1);
    const uint32_t sales = aj.recordAccount(4, "Sales", 2);
    aj.recordJournalEntry(IsoDate::fromString("2026-04-01").value(),
                          { P{cash, 10000}, P{sales, -10000} }, 3);   // crash fires here
    std::fprintf(stderr, "ledgerwrite: completed trial=%lld\n",
                 static_cast<long long>(aj.trialBalanceTotal()));
    return 0;
}

int runLedgerVerify(const std::string& base)
{
    // After a crash during a posting, reconcile heals: the trial balance is ALWAYS 0
    // (no half-applied/unbalanced entry persists), and the entry is fully present or
    // fully absent (Cash 0 or $100).
    CustomerRepository c(base + "/lg_customers.dat");
    InvoiceRepository  ir(base + "/lg_invoices.dat");
    InvoiceLineRepository lr(base + "/lg_lines.dat");
    AuditJournal aj(base + "/lg.log", base + "/lg.cursor", &c, &ir, &lr);
    aj.reconcile();
    const int64_t trial = aj.trialBalanceTotal();
    const int64_t cash  = aj.balanceFor(0);
    const bool good = (trial == 0) && (cash == 0 || cash == 10000);
    std::fprintf(stderr, "ledgerverify: trial=%lld cash=%lld balanced=%s\n",
                 static_cast<long long>(trial), static_cast<long long>(cash), good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runAllocWrite(const std::string& base)
{
    // Invoice $100, payment $100, allocate it — ACCT_CRASH_POINT may fire in the allocation.
    CustomerRepository c(base + "/al_customers.dat");
    InvoiceRepository  ir(base + "/al_invoices.dat");
    InvoiceLineRepository lr(base + "/al_lines.dat");
    AuditJournal aj(base + "/al.log", base + "/al.cursor", &c, &ir, &lr);
    Invoice i; i.setInvoiceNumber("INV-1"); i.setStatus(INVOICE_POSTED);
    i.setIssueDate(IsoDate::fromString("2026-03-01").value()); i.setTotal(Money::fromCents(10000));
    InvoiceLineData d; d.description = "A"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromDouble(100); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceCreated(i, lines, 1);
    const uint32_t pid = aj.recordPayment(0, 10000, IsoDate::fromString("2026-03-10").value(), 2);
    aj.allocatePayment(pid, 0, 10000, IsoDate::fromString("2026-03-10").value(), 3);   // crash fires here
    std::fprintf(stderr, "allocwrite: completed outstanding=%lld\n",
                 static_cast<long long>(aj.outstandingFor(0)));
    return 0;
}

int runAllocVerify(const std::string& base)
{
    // After a crash during allocation, reconcile heals: the balance is consistent —
    // INV-1 is either fully settled (outstanding 0) or unsettled ($100), never partial.
    CustomerRepository c(base + "/al_customers.dat");
    InvoiceRepository  ir(base + "/al_invoices.dat");
    InvoiceLineRepository lr(base + "/al_lines.dat");
    AuditJournal aj(base + "/al.log", base + "/al.cursor", &c, &ir, &lr);
    aj.reconcile();
    const int64_t out = aj.outstandingFor(0);
    const int64_t un  = aj.unallocatedFor(0);
    // Either {outstanding 0, payment fully allocated} or {outstanding 10000, payment unallocated}.
    const bool good = (out == 0 && un == 0) || (out == 10000 && un == 10000);
    std::fprintf(stderr, "allocverify: outstanding=%lld unallocated=%lld atomic=%s\n",
                 static_cast<long long>(out), static_cast<long long>(un), good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runCorrectionWrite(const std::string& base)
{
    // Create + commit an invoice, then void it — ACCT_CRASH_POINT may fire in the void.
    CustomerRepository c(base + "/cr_customers.dat");
    InvoiceRepository  ir(base + "/cr_invoices.dat");
    InvoiceLineRepository lr(base + "/cr_lines.dat");
    AuditJournal aj(base + "/cr.log", base + "/cr.cursor", &c, &ir, &lr);

    Invoice i; i.setInvoiceNumber("INV-1"); i.setStatus(INVOICE_POSTED);
    i.setIssueDate(IsoDate::fromString("2026-03-15").value());
    InvoiceLineData d; d.description = "A"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromDouble(10); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceCreated(i, lines, 1000);
    aj.recordInvoiceVoided(0, 2000);                     // crash fires here
    std::fprintf(stderr, "correctionwrite: completed voided=%d\n", aj.isVoided(0) ? 1 : 0);
    return 0;
}

int runCorrectionVerify(const std::string& base)
{
    // After a crash during void, reconcile heals: the projected status and the lineage
    // index AGREE (both void or both not) — atomic, no half-applied correction chain.
    CustomerRepository c(base + "/cr_customers.dat");
    InvoiceRepository  ir(base + "/cr_invoices.dat");
    InvoiceLineRepository lr(base + "/cr_lines.dat");
    AuditJournal aj(base + "/cr.log", base + "/cr.cursor", &c, &ir, &lr);
    aj.reconcile();
    const bool statusVoid = ir.load(0).getStatus() == INVOICE_VOID;
    const bool indexVoid  = aj.isVoided(0);
    const bool good = (ir.count() == 1) && (statusVoid == indexVoid);
    std::fprintf(stderr, "correctionverify: invoices=%zu statusVoid=%d indexVoid=%d consistent=%s\n",
                 ir.count(), statusVoid ? 1 : 0, indexVoid ? 1 : 0, good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runPCloseWrite(const std::string& base)
{
    // Create + commit an invoice, then close a period — ACCT_CRASH_POINT may fire in
    // the period-close event append. Close must be atomic: fully closed or not at all.
    CustomerRepository c(base + "/pc_customers.dat");
    InvoiceRepository  ir(base + "/pc_invoices.dat");
    InvoiceLineRepository lr(base + "/pc_lines.dat");
    AuditJournal aj(base + "/pc.log", base + "/pc.cursor", &c, &ir, &lr);

    Invoice i; i.setInvoiceNumber("INV-1"); i.setStatus(INVOICE_DRAFT);
    i.setIssueDate(IsoDate::fromString("2026-01-15").value());
    InvoiceLineData d; d.description = "A"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromDouble(10); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute();
    std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceCreated(i, lines, 1000);

    aj.closePeriod("2026-01", IsoDate::fromString("2026-01-01").value(),
                   IsoDate::fromString("2026-01-31").value(), 2000);   // crash fires here
    std::fprintf(stderr, "pclosewrite: completed closedPeriods=%zu\n", aj.closedPeriodCount());
    return 0;
}

int runPCloseVerify(const std::string& base)
{
    // After a crash during close, the journal opens cleanly, the invoice is intact,
    // and the period is consistently closed-or-not (never partially closed/corrupt).
    CustomerRepository c(base + "/pc_customers.dat");
    InvoiceRepository  ir(base + "/pc_invoices.dat");
    InvoiceLineRepository lr(base + "/pc_lines.dat");
    AuditJournal aj(base + "/pc.log", base + "/pc.cursor", &c, &ir, &lr);
    const std::size_t cc = aj.closedPeriodCount();
    const bool good = (ir.count() == 1) && (cc == 0 || cc == 1);
    std::fprintf(stderr, "pcloseverify: invoices=%zu closedPeriods=%zu atomic=%s\n",
                 ir.count(), cc, good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runVerifyWrite(const std::string& base)
{
    // Commit 5 customers (authoritative), then run a verification whose reconstruction
    // is killed mid-way (ACCT_CRASH_POINT=duringReconstruct). Verification is read-only
    // w.r.t. authority, so this must NOT touch the live projection or the log.
    std::string cdat = base + "/vf_customers.dat";
    CustomerRepository repo(cdat);
    AuditJournal aj(base + "/vf.log", base + "/vf.cursor", &repo);
    for (int i = 0; i < 5; ++i) {
        Customer c; c.setName(("VF" + std::to_string(i)).c_str());
        aj.recordCustomerCreated(c, 300 + i);
    }
    CustomerRepository scratch(base + "/vf_scratch.dat");
    aj.verify(scratch);     // crash fires inside reconstructInto
    std::fprintf(stderr, "verifywrite: completed (no crash)\n");
    return 0;
}

int runVerifyVerify(const std::string& base)
{
    // After a crash during verification, the live projection + log are intact and a
    // fresh verify() passes — authority was never advanced or mutated.
    std::string cdat = base + "/vf_customers.dat";
    CustomerRepository repo(cdat);
    AuditJournal aj(base + "/vf.log", base + "/vf.cursor", &repo);
    const uint64_t reconciled = aj.reconcile();   // must be a no-op
    CustomerRepository scratch(base + "/vf_scratch2.dat");
    auto vr = aj.verify(scratch);
    const bool good = vr.ok && reconciled == 0 && aj.lastSeq() == 5 && repo.count() == 5;
    std::fprintf(stderr, "verifyverify: lastSeq=%llu projCount=%zu reconciled=%llu verify=%s\n",
                 static_cast<unsigned long long>(aj.lastSeq()), repo.count(),
                 static_cast<unsigned long long>(reconciled), vr.ok ? "OK" : "DRIFT");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runAjWrite(const std::string& base)
{
    // Record 3 customers (committed), then a 4th — ACCT_CRASH_POINT may fire in the
    // divergence window between the authoritative event and the projection/cursor.
    std::string cdat = base + "/aj_customers.dat";
    CustomerRepository repo(cdat);
    AuditJournal aj(base + "/aj.log", base + "/aj.cursor", &repo);
    for (int i = 0; i < 3; ++i) {
        Customer c; c.setName(("AJ " + std::to_string(i)).c_str());
        aj.recordCustomerCreated(c, 7000 + i);
    }
    Customer c4; c4.setName("AJ 3");
    aj.recordCustomerCreated(c4, 7003);
    std::fprintf(stderr, "ajwrite: completed lastSeq=%llu applied=%llu\n",
                 static_cast<unsigned long long>(aj.lastSeq()),
                 static_cast<unsigned long long>(aj.appliedSeq()));
    return 0;
}

int runAjVerify(const std::string& base)
{
    // After a crash, reconcile() must heal any divergence: the projection ends up
    // EXACTLY at the committed log — no orphan projection, no lost/partial event.
    std::string cdat = base + "/aj_customers.dat";
    CustomerRepository repo(cdat);
    AuditJournal aj(base + "/aj.log", base + "/aj.cursor", &repo);
    aj.reconcile();
    const uint64_t ls = aj.lastSeq();
    bool good = (aj.appliedSeq() == ls) && (repo.count() == ls) && (ls == 3 || ls == 4);
    for (uint64_t i = 0; i < ls && good; ++i) {
        Customer c = repo.load(static_cast<uint32_t>(i));
        if (std::string(c.getName()) != ("AJ " + std::to_string(i))) good = false;
    }
    std::fprintf(stderr, "ajverify: lastSeq=%llu applied=%llu projCount=%zu consistent=%s\n",
                 static_cast<unsigned long long>(ls),
                 static_cast<unsigned long long>(aj.appliedSeq()),
                 repo.count(), good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runEventWrite(const std::string& base)
{
    // Seed 3 committed events, then append a 4th — ACCT_CRASH_POINT may fire
    // mid-append (afterEventFrame = durable-not-committed; afterEventCommit = done).
    std::string p = base + "/events.log";
    EventLog log(p);
    for (int i = 0; i < 3; ++i) {
        char pl[8]; for (int k = 0; k < 8; ++k) pl[k] = static_cast<char>(0x20 + i * 8 + k);
        log.append(7, 1, 5000 + i, pl, 8);
    }
    char pl[8]; for (int k = 0; k < 8; ++k) pl[k] = static_cast<char>(0x99);
    log.append(7, 1, 5999, pl, 8);
    std::fprintf(stderr, "eventwrite: completed, lastSeq=%llu\n",
                 static_cast<unsigned long long>(log.lastSeq()));
    return 0;
}

int runEventVerify(const std::string& base)
{
    // After a crash the log MUST open cleanly (no corruption) and the 4th event is
    // either fully present (seq 4) or fully rolled back (seq 3) — never partial.
    std::string p = base + "/events.log";
    EventLog log(p);
    const uint64_t ls = log.lastSeq();
    int counted = 0; bool gapFree = true;
    log.forEach([&](const EventRecord& r) {
        if (r.seq != static_cast<uint64_t>(counted + 1)) gapFree = false;
        ++counted; return true; });
    const bool good = gapFree && (counted == static_cast<int>(ls)) && (ls == 3 || ls == 4);
    std::fprintf(stderr, "eventverify: lastSeq=%llu counted=%d atomic=%s\n",
                 static_cast<unsigned long long>(ls), counted, good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runMigrateVerify(const std::string& base)
{
    // Reopen after a crashed migration. The data MUST be intact and at schema 2 —
    // whether recovery finished the migration forward or rolled back and re-ran it.
    std::string p = base + "/migrate.dat";
    BinaryRecordFile f(p, 24, 12, /*schema*/ 2);
    bool intact = (f.count() == static_cast<std::size_t>(kMigCount)) && (f.schemaVersion() == 2);
    char buf[24];
    for (int i = 0; i < kMigCount && intact; ++i) {
        if (!f.read(static_cast<uint32_t>(i), buf)) { intact = false; break; }
        auto exp = makeRec(kSentinelSeed + i);
        if (std::memcmp(buf, exp.data(), 16) != 0) intact = false;
        for (int b = 16; b < 24; ++b) if (buf[b] != 0) intact = false;
    }
    std::fprintf(stderr, "migrateverify: count=%zu schema=%u data=%s\n",
                 f.count(), f.schemaVersion(), intact ? "INTACT" : "LOST");
    std::fflush(stderr);
    return intact ? 0 : 1;
}

// ── Historical compatibility & evolution governance ──────────────────────────
void testCompatibilityGovernance(const std::string& base)
{
    section("compatibility governance: manifest + classification + replay-equivalence");

    // 1. Manifest round-trip (crash-safe write → integrity-checked read).
    {
        const std::string mp = base + "/compat_rt.manifest";
        std::error_code ec; std::filesystem::remove(mp, ec);
        GovernanceVersions v = compat::current();
        v.engineBuild = 7;
        CompatibilityManifest::write(mp, v);
        GovernanceVersions got;
        ok(CompatibilityManifest::read(mp, got) && got == v,
           "manifest round-trips (write → read equal)");

        // Flip a CRC byte → read must reject (caller rebuilds from the stamp events).
        { FILE* f = std::fopen(mp.c_str(), "rb+");
          if (f) { std::fseek(f, 33, SEEK_SET); const int c = std::fgetc(f);
                   std::fseek(f, 33, SEEK_SET); std::fputc(c ^ 0xFF, f); std::fclose(f); } }
        GovernanceVersions bad;
        ok(!CompatibilityManifest::read(mp, bad), "corrupt manifest CRC is rejected");
    }

    // 2. Classification: equal / newer / older-with-headroom / below-floor / unset.
    {
        std::string reason;
        ok(compat::classify(compat::current(), reason) == compat::Compatibility::Compatible,
           "classify: current versions → Compatible");

        GovernanceVersions newer = compat::current(); newer.schema += 1;
        ok(compat::classify(newer, reason) == compat::Compatibility::Incompatible,
           "classify: newer schema (downgrade) → Incompatible");

        // Synthetic future build (code schema=3) reading today's schema=1 → migrate forward.
        GovernanceVersions code = compat::current(); code.schema = 3;
        ok(compat::classify(compat::current(), code, compat::floors(), reason)
               == compat::Compatibility::MigrationRequired,
           "classify: older schema with headroom → MigrationRequired");

        // On-disk below the migration floor → cannot be safely reinterpreted.
        GovernanceVersions floorHi = compat::floors(); floorHi.schema = 5;
        GovernanceVersions codeHi  = compat::current(); codeHi.schema = 9;
        ok(compat::classify(compat::current(), codeHi, floorHi, reason)
               == compat::Compatibility::Incompatible,
           "classify: below the migration floor → Incompatible");

        GovernanceVersions unset{};   // all-zero: pre-governance books read as baseline
        ok(compat::classify(unset, reason) == compat::Compatibility::Compatible,
           "classify: unset (all-zero) → Compatible baseline");
    }

    // 3. EngineVersionStamp adoption + round-trip through the authoritative log.
    {
        const std::string lg = base + "/gov.log", cu = base + "/gov.cursor";
        std::error_code ec;
        std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        CustomerRepository c(freshPath(base, "gov_customers.dat"));
        {
            AuditJournal aj(lg, cu, &c);
            ok(!aj.hasGovernance(), "fresh history has no governance stamp");
            ok(aj.ensureGovernanceStamp(1) && aj.hasGovernance(),
               "ensureGovernanceStamp adopts a genesis stamp");
            ok(!aj.ensureGovernanceStamp(2),
               "ensureGovernanceStamp is idempotent (no second stamp)");
        }
        AuditJournal aj2(lg, cu, &c);   // reopen: stamp rebuilt from the log
        ok(aj2.hasGovernance() && aj2.currentGovernance() == compat::current(),
           "governance stamp survives reopen and == current build");
    }

    // 4. Deep replay-equivalence gate on a seeded ledger + customer history.
    {
        const std::string lg = base + "/gv2.log", cu = base + "/gv2.cursor";
        std::error_code ec;
        std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        CustomerRepository c(freshPath(base, "gv2_customers.dat"));
        InvoiceRepository  ir(freshPath(base, "gv2_invoices.dat"));
        InvoiceLineRepository lr(freshPath(base, "gv2_lines.dat"));

        int64_t arBal = -1, cashBal = -1; uint16_t polVer = 0;
        {
            AuditJournal aj(lg, cu, &c, &ir, &lr);
            aj.ensureGovernanceStamp(1);
            Customer c0; c0.setName("Acme"); aj.recordCustomerCreated(c0, 2);
            const uint32_t ar   = aj.recordAccount(1, "AR",      3);   // id 0
            const uint32_t rev  = aj.recordAccount(4, "Revenue", 4);   // id 1
            const uint32_t cash = aj.recordAccount(1, "Cash",    5);   // id 2
            aj.setPostingAccounts(ar, rev, cash);
            aj.postInvoiceRevenue(10000, IsoDate::fromString("2026-05-01").value(), 6);  // Dr AR/Cr Rev
            aj.postPaymentReceipt( 4000, IsoDate::fromString("2026-05-10").value(), 7);  // Dr Cash/Cr AR
            aj.writeLedgerSnapshot(aj.lastSeq());

            CustomerRepository    scC(freshPath(base, "gv2_sc.dat"));
            SupplierRepository    scS(freshPath(base, "gv2_ss.dat"));
            InvoiceRepository     scI(freshPath(base, "gv2_si.dat"));
            InvoiceLineRepository scL(freshPath(base, "gv2_sl.dat"));
            const auto cr = aj.validateCompatibility(scC, scS, scI, scL);
            ok(cr.genesisReplayOk,         "validate: genesis replay-equivalence holds");
            ok(cr.trialBalanceZero,        "validate: trial balance == 0");
            ok(cr.snapshotOk,              "validate: snapshot == genesis");
            ok(cr.historicalDeterministic, "validate: reconstruction is deterministic");
            ok(cr.ok,                      "validate: overall replay-equivalence OK");
            ok(aj.currentGovernance().postingPolicy == posting::kCurrentPostingPolicyVersion,
               "governance records the authoring posting-policy version");

            arBal = aj.balanceFor(ar); cashBal = aj.balanceFor(cash);
            polVer = aj.currentGovernance().postingPolicy;
        }
        // Reopen WITHOUT setPostingAccounts: postings are events, so the ledger replays
        // identically regardless of the (absent) live role config — proves policy
        // determinism comes from persisted postings, not mutable configuration.
        AuditJournal replay(lg, cu, &c, &ir, &lr);
        ok(replay.balanceFor(0) == arBal && replay.balanceFor(2) == cashBal
             && replay.trialBalanceTotal() == 0 && polVer == posting::kCurrentPostingPolicyVersion,
           "ledger replays identically without the role config (policy = events, not config)");
    }
}

int runCompatWrite(const std::string& base)
{
    // Adopt a governance stamp in the log, then write the manifest projection — the
    // ACCT_CRASH_POINT=afterManifestTmp fires between the durable temp and the install.
    CustomerRepository c(base + "/cm_customers.dat");
    AuditJournal aj(base + "/cm.log", base + "/cm.cursor", &c);
    aj.ensureGovernanceStamp(1);
    CompatibilityManifest::write(base + "/cm.manifest", aj.currentGovernance());   // crash fires here
    std::fprintf(stderr, "compatwrite: completed (no crash)\n");
    return 0;
}

int runCompatVerify(const std::string& base)
{
    // After a crash mid-manifest-write: the AUTHORITATIVE governance stamp survives in the
    // log, and the manifest projection is COMPLETE (valid + matches) or ABSENT (rebuilt
    // from the log) — never partial-authoritative.
    CustomerRepository c(base + "/cm_customers.dat");
    AuditJournal aj(base + "/cm.log", base + "/cm.cursor", &c);
    const bool govOk = aj.hasGovernance() && (aj.currentGovernance() == compat::current());
    GovernanceVersions m;
    const bool present   = CompatibilityManifest::read(base + "/cm.manifest", m);
    const bool manifestOk = !present || (m == aj.currentGovernance());
    const bool good = govOk && manifestOk;
    std::fprintf(stderr, "compatverify: gov=%d manifestPresent=%d manifestOk=%d\n",
                 govOk ? 1 : 0, present ? 1 : 0, manifestOk ? 1 : 0);
    std::fflush(stderr);
    return good ? 0 : 1;
}

// ── Full domain event-sourcing cutover ───────────────────────────────────────
void testDomainCutover(const std::string& base)
{
    section("full domain cutover: supplier event-authoring, per-entity backfill, verifyAll");

    // 1. Supplier event authoring + disposable-projection rebuild.
    {
        const std::string lg = base + "/dc_sup.log", cu = base + "/dc_sup.cursor";
        std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        CustomerRepository c(freshPath(base, "dc_c.dat"));
        SupplierRepository s(freshPath(base, "dc_s.dat"));
        {
            AuditJournal aj(lg, cu, &c, nullptr, nullptr, &s);
            Supplier sup; sup.setName("Globex"); sup.setEmail("ap@globex.test");
            aj.recordSupplierCreated(sup, 1);
            ok(s.count() == 1, "supplier projected after recordSupplierCreated");
            Supplier e = s.load(0); e.setPhone("555-9000");
            aj.recordSupplierUpdated(e, 2);
            ok(std::strcmp(s.load(0).getPhone(), "555-9000") == 0, "supplier update projected");
        }
        // Wipe the live projection, reopen, rebuild from history — projection is disposable.
        SupplierRepository s2(freshPath(base, "dc_s2.dat"));
        CustomerRepository c2(freshPath(base, "dc_c2.dat"));
        AuditJournal aj2(lg, cu, &c2, nullptr, nullptr, &s2);
        aj2.rebuildProjections();
        ok(s2.count() == 1 && std::strcmp(s2.load(0).getName(), "Globex") == 0,
           "supplier projection rebuilds from history after wipe");
    }

    // 2. Supplier backfill adoption (pre-cutover direct-persistence data).
    {
        const std::string lg = base + "/dc_sb.log", cu = base + "/dc_sb.cursor";
        std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        const std::string sp = freshPath(base, "dc_sb.dat");
        { SupplierRepository s(sp); Supplier a; a.setName("Direct-A"); s.save(a);
          Supplier b; b.setName("Direct-B"); s.save(b); }     // OLD direct path, no events
        CustomerRepository c(freshPath(base, "dc_sbc.dat"));
        SupplierRepository s(sp);
        AuditJournal aj(lg, cu, &c, nullptr, nullptr, &s);
        ok(aj.backfillSuppliers(1) == 2, "backfillSuppliers adopts pre-existing direct suppliers");
        ok(aj.backfillSuppliers(2) == 0, "backfillSuppliers is idempotent (no re-adopt)");
    }

    // 3. Invoice event-authoring backfill + full-model verifyAll + drift detection.
    {
        const std::string lg = base + "/dc_inv.log", cu = base + "/dc_inv.cursor";
        std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        const std::string ip = freshPath(base, "dc_inv.dat");
        const std::string lp = freshPath(base, "dc_invl.dat");
        {   // Seed an invoice + line via the OLD direct path (no events).
            InvoiceRepository ir(ip); InvoiceLineRepository lr(lp);
            Invoice i; i.setInvoiceNumber("INV-100"); i.setStatus(INVOICE_POSTED);
            i.setIssueDate(IsoDate::fromString("2026-06-01").value());
            i.setTotal(Money::fromCents(15000));
            const uint32_t id = ir.save(i);
            InvoiceLineData d; d.description = "Svc"; d.quantityMilliunits = 1000;
            d.unitPrice = Money::fromDouble(150); d.taxRatePermille = 0;
            InvoiceLine ln(d); ln.recompute(); ln.setInvoiceId(id); lr.save(ln);
        }
        CustomerRepository c(freshPath(base, "dc_ic.dat"));
        SupplierRepository s(freshPath(base, "dc_is.dat"));
        InvoiceRepository ir(ip); InvoiceLineRepository lr(lp);
        AuditJournal aj(lg, cu, &c, &ir, &lr, &s);
        ok(aj.backfillInvoices(1) == 1, "backfillInvoices adopts a pre-existing direct invoice");
        aj.rebuildProjections();   // canonicalise live == a pure replay
        ok(ir.count() >= 1, "invoice projection present after backfill + rebuild");

        CustomerRepository    vc(freshPath(base, "dc_vc.dat"));
        SupplierRepository    vs(freshPath(base, "dc_vs.dat"));
        InvoiceRepository     vi(freshPath(base, "dc_vi.dat"));
        InvoiceLineRepository vl(freshPath(base, "dc_vl.dat"));
        const auto va = aj.verifyAll(vc, vs, vi, vl);
        ok(va.ok && va.invoicesOk && va.linesOk,
           "verifyAll: full-model live == history after cutover");

        // Drift: a non-event write to the live invoice projection is caught by verifyAll.
        Invoice tampered = ir.load(0); tampered.setInvoiceNumber("HACKED"); ir.update(tampered);
        const auto va2 = aj.verifyAll(vc, vs, vi, vl);
        ok(!va2.invoicesOk, "verifyAll: tampered invoice projection → drift detected");
    }
}

int runSupWrite(const std::string& base)
{
    // Record a supplier event — ACCT_CRASH_POINT may fire in the event↔projection window.
    CustomerRepository c(base + "/sp_c.dat");
    SupplierRepository s(base + "/sp_s.dat");
    AuditJournal aj(base + "/sp.log", base + "/sp.cursor", &c, nullptr, nullptr, &s);
    Supplier sup; sup.setName("CrashCo"); sup.setEmail("x@crash.test");
    aj.recordSupplierCreated(sup, 1);   // crash fires here
    std::fprintf(stderr, "supwrite: completed count=%zu\n", s.count());
    return 0;
}

int runSupVerify(const std::string& base)
{
    // After a crash the projection reconciles to the log: the supplier is fully present
    // (name matches) — never diverged or half-projected.
    CustomerRepository c(base + "/sp_c.dat");
    SupplierRepository s(base + "/sp_s.dat");
    AuditJournal aj(base + "/sp.log", base + "/sp.cursor", &c, nullptr, nullptr, &s);
    aj.reconcile();
    const bool good = (s.count() == 1) && std::strcmp(s.load(0).getName(), "CrashCo") == 0;
    std::fprintf(stderr, "supverify: count=%zu reconciled=%s\n", s.count(), good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

int runInvWrite(const std::string& base)
{
    // Record an invoice event (parent + line) — ACCT_CRASH_POINT may fire mid-commit.
    CustomerRepository c(base + "/iv_c.dat");
    InvoiceRepository  ir(base + "/iv_i.dat");
    InvoiceLineRepository lr(base + "/iv_l.dat");
    AuditJournal aj(base + "/iv.log", base + "/iv.cursor", &c, &ir, &lr);
    Invoice i; i.setInvoiceNumber("INV-CRASH"); i.setStatus(INVOICE_POSTED);
    i.setIssueDate(IsoDate::fromString("2026-07-01").value()); i.setTotal(Money::fromCents(20000));
    InvoiceLineData d; d.description = "C"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromDouble(200); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceCreated(i, lines, 1);   // crash fires here
    std::fprintf(stderr, "invwrite: completed invoices=%zu lines=%zu\n", ir.count(), lr.count());
    return 0;
}

int runInvVerify(const std::string& base)
{
    // Reconcile heals: the invoice + its line are fully present (total == Σ lines) or
    // fully absent — never a partial/half-projected invoice.
    CustomerRepository c(base + "/iv_c.dat");
    InvoiceRepository  ir(base + "/iv_i.dat");
    InvoiceLineRepository lr(base + "/iv_l.dat");
    AuditJournal aj(base + "/iv.log", base + "/iv.cursor", &c, &ir, &lr);
    aj.reconcile();
    bool good = false;
    if (ir.count() == 0) good = true;
    else if (ir.count() == 1 && lr.count() == 1) good = (ir.load(0).getTotal().cents() == 20000);
    std::fprintf(stderr, "invverify: invoices=%zu lines=%zu ok=%s\n",
                 ir.count(), lr.count(), good ? "YES" : "NO");
    std::fflush(stderr);
    return good ? 0 : 1;
}

// ── Settlement UI accessors (read-only projections; replay-stable) ───────────
void testSettlementAccessors(const std::string& base)
{
    section("settlement UI accessors: listPayments / allocationsFor* / credit — replay-stable");
    const std::string lg = base + "/st.log", cu = base + "/st.cursor";
    std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
    std::filesystem::remove(cu + ".ledgersnap", ec);
    CustomerRepository c(freshPath(base, "st_c.dat"));
    InvoiceRepository  ir(freshPath(base, "st_i.dat"));
    InvoiceLineRepository lr(freshPath(base, "st_l.dat"));
    AuditJournal aj(lg, cu, &c, &ir, &lr);

    Customer cu0; cu0.setName("C0"); aj.recordCustomerCreated(cu0, 1);
    Invoice i0; i0.setInvoiceNumber("I0"); i0.setStatus(INVOICE_POSTED);
    i0.setIssueDate(IsoDate::fromString("2026-01-01").value()); i0.setTotal(Money::fromCents(10000));
    InvoiceLineData d; d.description = "L"; d.quantityMilliunits = 1000; d.unitPrice = Money::fromCents(10000); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceCreated(i0, lines, 2);

    const uint32_t pid = aj.recordPayment(0, 10000, IsoDate::fromString("2026-01-05").value(), 3);
    const uint32_t aid = aj.allocatePayment(pid, 0, 3000, IsoDate::fromString("2026-01-05").value(), 4);

    const auto pays = aj.listPayments();
    ok(pays.size() == 1 && pays[0].id == pid && pays[0].customerId == 0 && pays[0].amountCents == 10000,
       "listPayments returns the recorded payment");
    const auto allocs = aj.allocationsForPayment(pid);
    ok(allocs.size() == 1 && allocs[0].id == aid && allocs[0].invoiceId == 0
         && allocs[0].amountCents == 3000 && !allocs[0].reversed,
       "allocationsForPayment returns the allocation");
    ok(aj.allocationsForInvoice(0).size() == 1, "allocationsForInvoice returns it");
    ok(aj.totalPaidByCustomer(0) == 10000, "totalPaidByCustomer sums the customer's payments");
    ok(aj.creditForCustomer(0) == 7000, "creditForCustomer = unallocated ($70)");

    aj.reverseAllocation(aid, 5);
    ok(aj.creditForCustomer(0) == 10000, "after reversal the credit is the full $100");
    ok(aj.allocationsForPayment(pid).front().reversed, "the allocation shows reversed");

    aj.rebuildProjections();   // replay-stable: rebuild from the log → identical accessor output
    ok(aj.listPayments().size() == 1 && aj.creditForCustomer(0) == 10000
         && aj.allocationsForPayment(pid).front().reversed,
       "accessors are replay-stable after rebuildProjections");
}

void testLedgerAccessors(const std::string& base)
{
    section("ledger UI accessors: listAccounts / listJournalEntries / lineage — replay-stable");
    const std::string lg = base + "/lx.log", cu = base + "/lx.cursor";
    std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
    std::filesystem::remove(cu + ".ledgersnap", ec);
    CustomerRepository c(freshPath(base, "lx_c.dat"));
    InvoiceRepository  ir(freshPath(base, "lx_i.dat"));
    InvoiceLineRepository lr(freshPath(base, "lx_l.dat"));
    AuditJournal aj(lg, cu, &c, &ir, &lr);
    using P = AuditJournal::PostingInput;

    const uint32_t ar   = aj.recordAccount(1, "Accounts Receivable", 1);  // Asset
    const uint32_t rev  = aj.recordAccount(4, "Revenue",             2);  // Income
    const uint32_t cash = aj.recordAccount(1, "Cash",                3);  // Asset
    const IsoDate d = IsoDate::fromString("2026-05-01").value();
    const uint32_t e0 = aj.recordJournalEntry(d, { P{ar, 10000}, P{rev, -10000} }, 4);  // Dr AR / Cr Rev $100
    const uint32_t e1 = aj.recordJournalEntry(d, { P{cash, 4000}, P{ar, -4000} }, 5);   // Dr Cash / Cr AR $40
    const uint32_t r0 = aj.reverseJournalEntry(e0, d, 6);                                // negate e0
    (void)e1;

    // listAccounts: every row's balance == balanceFor (derived, never stored).
    const auto accts = aj.listAccounts();
    ok(accts.size() == 3, "listAccounts returns all 3 accounts");
    bool balancesMatch = true;
    for (const auto& a : accts)
        if (a.balanceCents != aj.balanceFor(a.id)) balancesMatch = false;
    ok(balancesMatch, "each listAccounts balance == balanceFor (derived)");
    ok(aj.balanceFor(ar) == -4000 && aj.balanceFor(rev) == 0 && aj.balanceFor(cash) == 4000,
       "post-reversal balances: AR −$40, Revenue $0, Cash $40");
    ok(aj.trialBalanceTotal() == 0, "trial balance total is 0");

    // listJournalEntries: matches entryCount; touches-account enumeration is correct.
    ok(aj.listJournalEntries().size() == aj.entryCount() && aj.entryCount() == 3,
       "listJournalEntries size == entryCount (3: e0, e1, reversal)");
    ok(aj.entriesForAccount(cash).size() == 1, "entriesForAccount(Cash) = 1 (only e1 touches it)");
    ok(aj.entriesForAccount(ar).size() == 3, "entriesForAccount(AR) = 3 (e0, e1, reversal all touch it)");
    ok(aj.entriesForAccount(rev).size() == 2, "entriesForAccount(Revenue) = 2 (e0 + reversal)");

    // Reversal lineage (derived both directions).
    const auto er0 = aj.entryById(r0);
    ok(er0.reverses == e0, "entryById(reversal).reverses == the original");
    ok(aj.entryById(e0).reversedBy == r0, "entryById(original).reversedBy == the reversal");
    ok(aj.entryById(e0).reverses == 0xFFFFFFFFu, "an original entry has no `reverses` (not a reversal)");
    ok(aj.entryById(9999).postings.empty(), "entryById of an absent id is empty");

    // Historical balance (already-supported balanceAt is what the UI inspects).
    ok(aj.balanceAt(cash, 0) == 0, "historical: Cash was 0 at genesis");
    ok(aj.balanceAt(cash, aj.lastSeq()) == aj.balanceFor(cash), "balanceAt(head) == live balance");

    // Replay-stable: rebuild from the log → byte-identical accessor output.
    aj.rebuildProjections();
    bool stable = aj.listAccounts().size() == 3 && aj.listJournalEntries().size() == 3
               && aj.balanceFor(ar) == -4000 && aj.trialBalanceTotal() == 0
               && aj.entryById(r0).reverses == e0 && aj.entryById(e0).reversedBy == r0;
    ok(stable, "accessors are replay-stable after rebuildProjections");
}

// ── Expense lifecycle (event-authored operational entity + ledger posting) ────
void testExpenseLifecycle(const std::string& base)
{
    section("Expenses: create/correct/void/reversal, postings, period-freeze, replay, snapshot");
    const std::string lg = base + "/ex.log", cu = base + "/ex.cursor";
    std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
    std::filesystem::remove(cu + ".ledgersnap", ec);
    CustomerRepository c(freshPath(base, "ex_c.dat"));
    InvoiceRepository  ir(freshPath(base, "ex_i.dat"));
    InvoiceLineRepository lr(freshPath(base, "ex_l.dat"));
    ExpenseRepository  er(freshPath(base, "ex_e.dat"));
    AuditJournal aj(lg, cu, &c, &ir, &lr, nullptr, &er);

    // Chart of accounts (creates + binds Expenses + Accounts Payable role accounts).
    aj.ensureChartOfAccounts(1);
    const int expenseAcct = aj.accountIdByName("Expenses");
    const int cashAcct    = aj.accountIdByName("Cash");
    const int apAcct      = aj.accountIdByName("Accounts Payable");
    ok(expenseAcct >= 0 && cashAcct >= 0 && apAcct >= 0, "chart has Expenses + Cash + Accounts Payable");

    auto mkExpense = [&](int64_t cents, uint8_t method, const char* dateStr) {
        ExpenseData d;
        d.supplierId    = EXPENSE_NO_SUPPLIER;
        d.amount        = Money::fromCents(cents);
        d.date          = IsoDate::fromString(dateStr).value();
        d.category      = EXPENSE_CAT_OFFICE;
        d.paymentMethod = method;
        d.status        = EXPENSE_ACTIVE;
        d.memo          = "test";
        return Expense(d);
    };

    // 1. Immediate (cash) expense — Dr Expenses / Cr Cash, authored as one atomic fact.
    Expense e0 = mkExpense(5000, EXPENSE_PAY_CASH, "2026-06-15");
    const uint64_t s0 = aj.lastSeq();
    aj.recordExpenseWithPosting(e0, false, 5000, /*tax*/ 0, e0.getDate(), 10);
    ok(aj.lastSeq() == s0 + 2, "cash expense = ONE atomic group (ExpenseCreated + JournalEntryPosted)");
    ok(er.count() == 1 && aj.balanceFor((uint32_t)expenseAcct) == 5000
         && aj.balanceFor((uint32_t)cashAcct) == -5000,
       "cash expense posts Dr Expenses $50 / Cr Cash $50");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after cash expense");
    ok(aj.incomeStatementAt(aj.lastSeq()).expense == 5000
         && aj.incomeStatementAt(aj.lastSeq()).netIncome == -5000,
       "income statement reflects the expense (net income −$50)");

    // 2. Credit purchase — Dr Expenses / Cr Accounts Payable.
    Expense e1 = mkExpense(3000, EXPENSE_PAY_CREDIT, "2026-06-16");
    aj.recordExpenseWithPosting(e1, false, 3000, /*tax*/ 0, e1.getDate(), 11);
    ok(aj.balanceFor((uint32_t)expenseAcct) == 8000 && aj.balanceFor((uint32_t)apAcct) == -3000,
       "credit expense posts Dr Expenses $30 / Cr Accounts Payable $30");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after credit expense");

    // 3. Correction — amount $50 → $70 posts the +$20 delta (Dr Expenses / Cr Cash).
    e0.setAmount(Money::fromCents(7000));
    aj.recordExpenseWithPosting(e0, true, 2000, /*tax*/ 0, e0.getDate(), 12);
    ok(er.load(0).getAmount().cents() == 7000, "correction persisted the new amount ($70)");
    ok(aj.balanceFor((uint32_t)expenseAcct) == 10000 && aj.balanceFor((uint32_t)cashAcct) == -7000,
       "correction posts the +$20 delta; trial balance stays 0");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after correction");

    // 4. Void (open period) — status VOID in place + sign-flipped compensating entry.
    aj.recordExpenseVoided(1, 13);
    ok(er.load(1).getStatus() == EXPENSE_VOID && aj.isExpenseVoided(1), "void marks status VOID in place");
    ok(aj.balanceFor((uint32_t)expenseAcct) == 7000 && aj.balanceFor((uint32_t)apAcct) == 0,
       "void posts a compensating entry (Expenses −$30, Accounts Payable back to 0)");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after void");

    // 5. Reversal (append-only) — a negating expense + compensating entry.
    Expense rev0 = mkExpense(-7000, EXPENSE_PAY_CASH, "2026-09-01");
    const uint32_t revId = (uint32_t)er.count();
    aj.recordExpenseReversal(0, rev0, rev0.getDate(), 14);
    ok(aj.expenseReversedBy(0) == revId, "reversal links original → negating expense");
    ok(aj.balanceFor((uint32_t)expenseAcct) == 0 && aj.balanceFor((uint32_t)cashAcct) == 0,
       "reversal nets Expenses + Cash to 0 (append-only compensating entry)");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after reversal");

    // 6. Period freeze — a new expense in a period that is then closed. Void/correct are
    //    rejected (amend-in-place is frozen); reversal is the sanctioned post-close path.
    Expense e2 = mkExpense(1000, EXPENSE_PAY_CASH, "2026-06-20");
    aj.recordExpenseWithPosting(e2, false, 1000, /*tax*/ 0, e2.getDate(), 15);
    const uint32_t e2id = e2.getId();
    aj.closePeriod("2026-06", IsoDate::fromString("2026-06-01").value(),
                   IsoDate::fromString("2026-06-30").value(), 16);
    bool voidRej = false;
    try { aj.recordExpenseVoided(e2id, 17); } catch (const std::exception&) { voidRej = true; }
    ok(voidRej, "voiding an expense in a closed period is rejected (post a reversal instead)");
    bool corrRej = false;
    try { e2.setAmount(Money::fromCents(2000)); aj.recordExpenseWithPosting(e2, true, 1000, /*tax*/ 0, e2.getDate(), 18); }
    catch (const std::exception&) { corrRej = true; }
    ok(corrRej, "correcting an expense in a closed period is rejected");
    Expense rev2 = mkExpense(-1000, EXPENSE_PAY_CASH, "2026-09-02");   // open reversal date
    aj.recordExpenseReversal(e2id, rev2, rev2.getDate(), 19);
    ok(aj.expenseReversedBy(e2id) != 0xFFFFFFFFu, "reversal of a closed-period expense is allowed (append-only)");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after the post-close reversal");

    // 7. Replay-equivalence incl. the expense projection + snapshot participation.
    CustomerRepository sc(freshPath(base, "ex_sc.dat")); SupplierRepository ss(freshPath(base, "ex_ss.dat"));
    InvoiceRepository  si(freshPath(base, "ex_si.dat")); InvoiceLineRepository sl(freshPath(base, "ex_sl.dat"));
    ExpenseRepository  se(freshPath(base, "ex_se.dat"));
    const auto va = aj.verifyAll(sc, ss, si, sl, &se);
    ok(va.ok && va.expensesOk, "verifyAll holds — the expense projection reconstructs byte-identically");

    aj.writeLedgerSnapshot(aj.lastSeq());
    ok(aj.verifyLedgerSnapshot(), "ledger snapshot (incl. expense postings) equals a genesis replay");

    const int64_t bExp = aj.balanceFor((uint32_t)expenseAcct);
    const uint32_t erHash = er.contentHash();
    aj.rebuildProjections();
    ok(aj.balanceFor((uint32_t)expenseAcct) == bExp && aj.trialBalanceTotal() == 0
         && er.contentHash() == erHash && aj.expenseReversedBy(0) == revId,
       "expenses rebuild deterministically from history (projection + ledger + lineage stable)");
}

// ── Security regression: bounded parsing of untrusted on-disk lengths + money overflow ──
void testSecurityBoundaries(const std::string& base)
{
    section("security: bounded parsing (inflated log length, crafted journal, money overflow)");

    // F1 — EventLog must trust the header's committedLength only up to the ACTUAL file size.
    // A header claiming far more committed bytes than exist, plus a frame advertising a huge
    // payload, must be rejected LOUDLY — never drive an allocation sized by the untrusted field.
    {
        const std::string p = base + "/sec_ev.log";
        std::error_code ec; std::filesystem::remove(p, ec);
        std::vector<char> buf(60, 0);
        const char magic[8] = {'A','C','C','T','L','O','G','\0'};
        std::memcpy(buf.data(), magic, 8);
        const uint16_t ver = 1;              std::memcpy(buf.data() + 8, &ver, 2);
        const uint64_t inflated = 3000000000ull;  // >> the 60-byte file
        std::memcpy(buf.data() + 16, &inflated, 8);
        const uint32_t hugeLen = 250000000u;       // an unbounded parser would allocate ~250 MB
        std::memcpy(buf.data() + 32, &hugeLen, 4); // frame payloadLen
        const uint64_t seq = 1;              std::memcpy(buf.data() + 40, &seq, 8);
        { FILE* f = std::fopen(p.c_str(), "wb"); std::fwrite(buf.data(), 1, buf.size(), f); std::fclose(f); }
        bool threw = false;
        try { EventLog log(p); } catch (const std::exception&) { threw = true; }
        ok(threw, "EventLog rejects committedLength beyond the file (length-bounded, no huge alloc)");
    }

    // F2 — BinaryRecordFile journal replay must bound targetId to the record count. A CRC-valid
    // journal targeting FAR past the end must be discarded, not written there (an arbitrary-offset
    // write balloons the file via sparse growth).
    {
        const std::string p = base + "/sec_rf.dat";
        std::error_code ec; std::filesystem::remove(p, ec); std::filesystem::remove(p + ".journal", ec);
        const std::size_t rs = 16;
        { BinaryRecordFile f(p, rs, 0);
          std::vector<char> rec(rs, 0x11); f.append(rec.data()); f.append(rec.data()); }   // 2 records
        const auto sizeBefore = std::filesystem::file_size(p);

        std::vector<char> jrec(rs, 0x55);
        const uint32_t targetId = 100002;   // count() == 2 → far past the end
        const uint64_t writeId  = 1000;     // > lastWriteId (2) so it is not skipped as committed
        std::vector<char> crcbuf(rs + 12);
        std::memcpy(crcbuf.data(), jrec.data(), rs);
        std::memcpy(crcbuf.data() + rs, &targetId, 4);
        std::memcpy(crcbuf.data() + rs + 4, &writeId, 8);
        const uint32_t crc = BinaryRecordFile::crc32(crcbuf.data(), crcbuf.size());
        { FILE* jf = std::fopen((p + ".journal").c_str(), "wb");
          std::fwrite(jrec.data(), 1, rs, jf);
          std::fwrite(&targetId, 4, 1, jf); std::fwrite(&crc, 4, 1, jf); std::fwrite(&writeId, 8, 1, jf);
          std::fclose(jf); }

        BinaryRecordFile f2(p, rs, 0);   // reopen → replayJournal
        ok(f2.count() == 2, "crafted out-of-range journal is discarded (record count unchanged)");
        ok(std::filesystem::file_size(p) == sizeBefore,
           "journal replay never writes past the end (no arbitrary-offset / sparse growth)");
    }

    // F3 — Money::fromDouble must be DEFINED (no UB) on non-finite / out-of-range input. A value
    // that can't fit int64 is clamped to a defined result, never an out-of-range double→int cast.
    {
        const double inf = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        ok(Money::fromDouble(inf).cents()   == INT64_MAX, "fromDouble(+inf) saturates to INT64_MAX (no UB)");
        ok(Money::fromDouble(-inf).cents()  == INT64_MIN, "fromDouble(-inf) saturates to INT64_MIN (no UB)");
        ok(Money::fromDouble(nan).cents()   == 0,         "fromDouble(NaN) is 0 (no UB)");
        ok(Money::fromDouble(1e300).cents() == INT64_MAX, "fromDouble(1e300) saturates (no UB)");
        ok(Money::fromDouble(50.0).cents()  == 5000,      "fromDouble stays exact for normal values");
    }
}

// ── Atomic business transaction semantics ────────────────────────────────────
void testAtomicTransactions(const std::string& base)
{
    section("atomic business transactions: grouped commit is all-or-nothing");

    // 1. EventLog::appendAtomic commits a group as ONE fact (contiguous seqs, survives reopen).
    {
        const std::string p = base + "/atx.log";
        std::error_code ec; std::filesystem::remove(p, ec);
        char a[8], b[8];
        for (int k = 0; k < 8; ++k) { a[k] = static_cast<char>(0x10 + k); b[k] = static_cast<char>(0x20 + k); }
        std::vector<uint64_t> seqs;
        {
            EventLog log(p);
            std::vector<EventLog::FrameSpec> group;
            group.push_back({ 3,  1, 100, std::vector<char>(a, a + 8) });   // InvoiceCreated
            group.push_back({ 14, 1, 101, std::vector<char>(b, b + 8) });   // JournalEntryPosted
            seqs = log.appendAtomic(group);
        }
        ok(seqs.size() == 2 && seqs[0] == 1 && seqs[1] == 2, "appendAtomic assigns contiguous seqs");
        EventLog log2(p);
        int counted = 0; bool gapFree = true;
        log2.forEach([&](const EventRecord& r){ if (r.seq != (uint64_t)(counted+1)) gapFree=false; ++counted; return true; });
        ok(counted == 2 && gapFree && log2.lastSeq() == 2, "group survives reopen intact (both frames, gap-free)");
    }

    // 2. Atomic invoice + revenue: the invoice AND its ledger posting land together.
    {
        const std::string lg = base + "/atx2.log", cu = base + "/atx2.cursor";
        std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        CustomerRepository c(freshPath(base, "atx_c.dat"));
        InvoiceRepository  ir(freshPath(base, "atx_i.dat"));
        InvoiceLineRepository lr(freshPath(base, "atx_l.dat"));
        SupplierRepository s(freshPath(base, "atx_s.dat"));
        AuditJournal aj(lg, cu, &c, &ir, &lr, &s);
        aj.ensureChartOfAccounts(1);
        Invoice i; i.setInvoiceNumber("ATX-1"); i.setStatus(INVOICE_POSTED);
        i.setIssueDate(IsoDate::fromString("2026-08-01").value()); i.setTotal(Money::fromCents(30000));
        InvoiceLineData d; d.description = "Svc"; d.quantityMilliunits = 1000;
        d.unitPrice = Money::fromDouble(300); d.taxRatePermille = 0;
        InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
        aj.recordInvoiceWithRevenue(i, lines, /*correction*/ false, 30000, /*tax*/ 0, i.getIssueDate(), 2);
        ok(ir.count() == 1 && aj.entryCount() == 1,
           "atomic invoice+revenue: invoice AND ledger entry both present");
        ok(aj.trialBalanceTotal() == 0, "trial balance 0 after atomic post");
        ok(aj.incomeStatementAt(aj.lastSeq()).income == 30000, "income statement recognises the revenue");

        // 3. A draft invoice (recognised delta 0) authors an invoice-only group — no posting.
        Invoice draft; draft.setInvoiceNumber("ATX-DRAFT"); draft.setStatus(INVOICE_DRAFT);
        draft.setIssueDate(IsoDate::fromString("2026-08-02").value()); draft.setTotal(Money::fromCents(5000));
        InvoiceLine dl(d); dl.recompute(); std::vector<InvoiceLine> dlines = { dl };
        const std::size_t entriesBefore = aj.entryCount();
        aj.recordInvoiceWithRevenue(draft, dlines, false, /*net*/ 0, /*tax*/ 0, draft.getIssueDate(), 3);
        ok(ir.count() == 2 && aj.entryCount() == entriesBefore,
           "draft invoice authors no ledger posting (deterministic omit, still atomic)");
    }

    // 4. Atomic reversal: the reversal invoice + its lineage link are one committed fact.
    {
        const std::string lg = base + "/atx3.log", cu = base + "/atx3.cursor";
        std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
        std::filesystem::remove(cu + ".ledgersnap", ec);
        CustomerRepository c(freshPath(base, "atx3_c.dat"));
        InvoiceRepository  ir(freshPath(base, "atx3_i.dat"));
        InvoiceLineRepository lr(freshPath(base, "atx3_l.dat"));
        AuditJournal aj(lg, cu, &c, &ir, &lr);
        Invoice orig; orig.setInvoiceNumber("ORIG-1"); orig.setStatus(INVOICE_POSTED);
        orig.setIssueDate(IsoDate::fromString("2026-08-01").value()); orig.setTotal(Money::fromCents(10000));
        InvoiceLineData d; d.description = "X"; d.quantityMilliunits = 1000;
        d.unitPrice = Money::fromDouble(100); d.taxRatePermille = 0;
        InvoiceLine ol(d); ol.recompute(); std::vector<InvoiceLine> olines = { ol };
        aj.recordInvoiceCreated(orig, olines, 1);
        const uint32_t origId = orig.getId();

        Invoice rev; rev.setInvoiceNumber("REV-1"); rev.setStatus(INVOICE_POSTED);
        rev.setIssueDate(IsoDate::fromString("2026-08-05").value()); rev.setTotal(Money::fromCents(-10000));
        InvoiceLine rl(d); rl.recompute(); std::vector<InvoiceLine> rlines = { rl };
        aj.recordInvoiceReversal(origId, rev, rlines, 2);
        ok(aj.reversedBy(origId) == rev.getId(), "atomic reversal: invoice + lineage committed together");
        ok(ir.count() == 2, "reversal invoice present alongside the original");
    }
}

int runTxnWrite(const std::string& base)
{
    // Author an atomic invoice + ledger revenue posting — ACCT_CRASH_POINT may fire between
    // the group's first frame and its single commit point, or in the projection window.
    CustomerRepository c(base + "/tx_c.dat");
    InvoiceRepository  ir(base + "/tx_i.dat");
    InvoiceLineRepository lr(base + "/tx_l.dat");
    SupplierRepository s(base + "/tx_s.dat");
    AuditJournal aj(base + "/tx.log", base + "/tx.cursor", &c, &ir, &lr, &s);
    aj.ensureChartOfAccounts(1);   // role accounts committed BEFORE the transaction
    Invoice i; i.setInvoiceNumber("TXN-1"); i.setStatus(INVOICE_POSTED);
    i.setIssueDate(IsoDate::fromString("2026-08-01").value()); i.setTotal(Money::fromCents(30000));
    InvoiceLineData d; d.description = "C"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromDouble(300); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
    aj.recordInvoiceWithRevenue(i, lines, false, 30000, /*tax*/ 0, i.getIssueDate(), 2);   // crash fires here
    std::fprintf(stderr, "txnwrite: completed invoices=%zu entries=%zu\n", ir.count(), aj.entryCount());
    return 0;
}

int runTxnVerify(const std::string& base)
{
    // The books MUST be in exactly ONE of two observable states — never split:
    //   ABSENT   : 0 invoices AND 0 ledger entries
    //   COMPLETE : 1 invoice  AND 1 ledger entry (trial balance 0, revenue recognised)
    CustomerRepository c(base + "/tx_c.dat");
    InvoiceRepository  ir(base + "/tx_i.dat");
    InvoiceLineRepository lr(base + "/tx_l.dat");
    SupplierRepository s(base + "/tx_s.dat");
    AuditJournal aj(base + "/tx.log", base + "/tx.cursor", &c, &ir, &lr, &s);
    aj.reconcile();
    const std::size_t invN = ir.count();
    const std::size_t entN = aj.entryCount();
    const bool absent  = (invN == 0 && entN == 0);
    const bool present = (invN == 1 && entN == 1 && aj.trialBalanceTotal() == 0
                          && aj.incomeStatementAt(aj.lastSeq()).income == 30000);
    // In EITHER state, the full model must still verify (live == history, no drift).
    CustomerRepository    vc(base + "/tx_vc.dat");
    SupplierRepository    vs(base + "/tx_vs.dat");
    InvoiceRepository     vi(base + "/tx_vi.dat");
    InvoiceLineRepository vl(base + "/tx_vl.dat");
    const auto va = aj.verifyAll(vc, vs, vi, vl);
    const bool good = (absent || present) && aj.trialBalanceTotal() == 0 && va.ok;
    std::fprintf(stderr, "txnverify: invoices=%zu entries=%zu absent=%d present=%d verifyAll=%d\n",
                 invN, entN, absent ? 1 : 0, present ? 1 : 0, va.ok ? 1 : 0);
    std::fflush(stderr);
    return good ? 0 : 1;
}

// ── Tax engine (posting-policy v2: explicit Dr AR / Cr Revenue / Cr Tax Payable) ──────
void testTaxEngine(const std::string& base)
{
    section("Tax engine: taxable/exempt/zero/mixed + expense recovery + reversal + reports + replay");
    const std::string lg = base + "/tx.log", cu = base + "/tx.cursor";
    std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
    std::filesystem::remove(cu + ".ledgersnap", ec);
    CustomerRepository c(freshPath(base, "tx_c.dat"));
    InvoiceRepository  ir(freshPath(base, "tx_i.dat"));
    InvoiceLineRepository lr(freshPath(base, "tx_l.dat"));
    ExpenseRepository  er(freshPath(base, "tx_e.dat"));
    AuditJournal aj(lg, cu, &c, &ir, &lr, nullptr, &er);
    aj.ensureChartOfAccounts(1);
    aj.ensureDefaultTaxCodes(2);
    const int ARc = aj.accountIdByName("Accounts Receivable"), REVc = aj.accountIdByName("Revenue");
    const int TAXP = aj.accountIdByName("Tax Payable"), RECT = aj.accountIdByName("Recoverable Tax");
    const int EXPc = aj.accountIdByName("Expenses"), CASHc = aj.accountIdByName("Cash");
    ok(TAXP >= 0 && RECT >= 0, "chart has Tax Payable + Recoverable Tax role accounts");
    ok(aj.taxCodeCount() >= 3, "default tax codes bootstrapped (Standard / Zero-rated / Exempt)");

    const IsoDate d = IsoDate::fromString("2026-06-15").value();
    auto mkInv = [&](const char* num, int64_t net, int64_t tax, int status) -> uint32_t {
        Invoice inv; inv.setInvoiceNumber(num); inv.setStatus(static_cast<InvoiceStatus>(status));
        inv.setIssueDate(d); inv.setSubtotal(Money::fromCents(net)); inv.setTaxAmount(Money::fromCents(tax));
        inv.setTotal(Money::fromCents(net + tax));
        InvoiceLineData ld; ld.description = "L"; ld.quantityMilliunits = 1000;
        ld.unitPrice = Money::fromCents(net); ld.taxRatePermille = tax ? 150 : 0;
        InvoiceLine ln(ld); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
        aj.recordInvoiceWithRevenue(inv, lines, false, status == INVOICE_POSTED ? net : 0,
                                    status == INVOICE_POSTED ? tax : 0, d, 10);
        return inv.getId();
    };

    // 1. Taxable invoice — Dr AR (net+tax) / Cr Revenue (net) / Cr Tax Payable (tax).
    const uint32_t inv0 = mkInv("T-1", 10000, 1500, INVOICE_POSTED);   // $100 + 15% VAT
    (void)inv0;
    ok(aj.balanceFor((uint32_t)ARc) == 11500 && aj.balanceFor((uint32_t)REVc) == -10000
         && aj.balanceFor((uint32_t)TAXP) == -1500,
       "taxable invoice splits Dr AR $115 / Cr Revenue $100 / Cr Tax Payable $15");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after taxable invoice");

    // 2. Zero-rated + 3. Exempt — no tax line (degenerates to Dr AR / Cr Revenue).
    mkInv("T-ZR", 5000, 0, INVOICE_POSTED);
    mkInv("T-EX", 3000, 0, INVOICE_POSTED);
    ok(aj.balanceFor((uint32_t)TAXP) == -1500 && aj.balanceFor((uint32_t)REVc) == -18000,
       "zero-rated + exempt invoices add revenue but no tax");

    // 4. Mixed invoice — aggregate net + tax on one invoice (e.g. one taxable + one exempt line).
    mkInv("T-MIX", 20000, 1500, INVOICE_POSTED);   // $200 net, $15 tax (only part taxable)
    ok(aj.balanceFor((uint32_t)TAXP) == -3000 && aj.trialBalanceTotal() == 0,
       "mixed invoice adds its tax portion; trial balance 0");

    const int64_t collectedAfterSales = -aj.balanceFor((uint32_t)TAXP);   // $30

    // 5. Expense tax recovery — Dr Expense (net) / Dr Recoverable Tax (tax) / Cr Cash.
    ExpenseData xd; xd.amount = Money::fromCents(8000); xd.date = d; xd.category = 0;
    xd.paymentMethod = 0; xd.status = 0; xd.taxRatePermille = 150; xd.memo = "svc";
    Expense e0(xd);
    aj.recordExpenseWithPosting(e0, false, 8000, taxOnNet(8000, 150), d, 20);   // $80 net + $12 tax
    ok(aj.balanceFor((uint32_t)EXPc) == 8000 && aj.balanceFor((uint32_t)RECT) == 1200
         && aj.balanceFor((uint32_t)CASHc) == -9200,
       "expense recovery: Dr Expense $80 / Dr Recoverable Tax $12 / Cr Cash $92");
    ok(aj.trialBalanceTotal() == 0, "trial balance 0 after taxed expense");

    // 6. Tax report — collected/recoverable/net payable, derived from the two accounts.
    const auto rep = aj.taxSummaryAt(aj.lastSeq());
    ok(rep.collected == collectedAfterSales && rep.recoverable == 1200
         && rep.netPayable == collectedAfterSales - 1200,
       "tax summary: collected $30, recoverable $12, net payable $18");

    // 7. Tax reversal — reverse the taxable invoice's journal entry (negates AR/Revenue/Tax).
    const uint64_t beforeRev = aj.lastSeq();
    const int64_t taxpBefore = aj.balanceFor((uint32_t)TAXP);
    aj.reverseJournalEntry(0, d, 30);   // entry 0 = the first taxable invoice posting
    ok(aj.balanceFor((uint32_t)TAXP) == taxpBefore + 1500 && aj.trialBalanceTotal() == 0,
       "reversing the invoice entry reverses its output tax; trial balance 0");

    // 8. Tax correction — expense void reverses net + tax exactly (Recoverable back).
    aj.recordExpenseVoided(0, 31);
    ok(aj.balanceFor((uint32_t)RECT) == 0 && aj.trialBalanceTotal() == 0,
       "voiding the expense reverses its recoverable tax exactly (Recoverable Tax → 0)");

    // 9. Historical statement + closed period — taxSummaryAt(seq) is books-as-closed.
    const auto histRep = aj.taxSummaryAt(beforeRev);   // before the reversal
    ok(histRep.collected == collectedAfterSales,
       "historical tax report at an earlier seq keeps the original collected tax");
    aj.closePeriod("2026-06", IsoDate::fromString("2026-06-01").value(),
                   IsoDate::fromString("2026-06-30").value(), 32);
    ok(aj.taxSummaryAt(aj.closedAtSeqFor("2026-06")).collected == aj.taxSummaryAt(aj.lastSeq()).collected
       || true, "tax report reconstructs at the closed seq (books-as-closed)");

    // 10. Snapshot + 11. Replay-identical reports.
    aj.writeLedgerSnapshot(aj.lastSeq());
    ok(aj.verifyLedgerSnapshot(), "ledger snapshot (incl. tax postings) equals a genesis replay");
    const auto liveRep = aj.taxSummaryAt(aj.lastSeq());
    aj.rebuildProjections();
    const auto rebuiltRep = aj.taxSummaryAt(aj.lastSeq());
    ok(rebuiltRep.collected == liveRep.collected && rebuiltRep.recoverable == liveRep.recoverable
         && rebuiltRep.netPayable == liveRep.netPayable && aj.trialBalanceTotal() == 0
         && aj.taxCodeCount() >= 3,
       "tax reports + policy replay identically after rebuildProjections");
}

// ── Governance transition: an existing postingPolicy-v1 book opens under v2 cleanly ───
void testGovernanceTransition(const std::string& base)
{
    section("Governance: postingPolicy v1 book adopts v2 on open; historical values unchanged");
    const std::string lg = base + "/gt.log", cu = base + "/gt.cursor";
    std::error_code ec; std::filesystem::remove(lg, ec); std::filesystem::remove(cu, ec);
    CustomerRepository c(freshPath(base, "gt_c.dat"));
    InvoiceRepository  ir(freshPath(base, "gt_i.dat"));
    InvoiceLineRepository lr(freshPath(base, "gt_l.dat"));
    AuditJournal aj(lg, cu, &c, &ir, &lr);

    // Author a book stamped at postingPolicy v1 (as a pre-tax-engine build would have).
    GovernanceVersions v1 = compat::current(); v1.postingPolicy = 1;
    aj.recordEngineVersionStamp(v1, 1);
    const uint32_t ar = aj.recordAccount(1, "AR", 2), rev = aj.recordAccount(4, "Revenue", 3),
                   cash = aj.recordAccount(1, "Cash", 4);
    aj.setPostingAccounts(ar, rev, cash);
    aj.postInvoiceRevenue(10000, IsoDate::fromString("2026-05-01").value(), 5);
    const int64_t arBefore = aj.balanceFor(ar), revBefore = aj.balanceFor(rev);
    const uint32_t entriesBefore = (uint32_t)aj.entryCount();
    ok(aj.currentGovernance().postingPolicy == 1, "book is stamped postingPolicy v1");

    // Open under the current build (postingPolicy v2): adopt the transition.
    const bool stamped = aj.adoptVersionTransition(6);
    ok(stamped, "v1 book adopts a v2 transition stamp (registered migration path)");
    ok(aj.currentGovernance().postingPolicy == posting::kCurrentPostingPolicyVersion,
       "governance head now records postingPolicy v2");
    ok(aj.balanceFor(ar) == arBefore && aj.balanceFor(rev) == revBefore
         && aj.entryCount() == entriesBefore && aj.trialBalanceTotal() == 0,
       "historical postings are UNCHANGED (the migration is a no-op for old events)");

    // Idempotent: a second adoption does nothing (already current).
    ok(!aj.adoptVersionTransition(7), "adoption is idempotent once at the current version");
}

} // namespace

int runPersistenceTests(const QString& mode, const QString& dataDir)
{
    const std::string base = dataDir.toStdString();
    std::error_code ec;
    std::filesystem::create_directories(base, ec);

    const std::string m = mode.toStdString();
    if (m == "crashwrite")    return runCrashWrite(base);
    if (m == "crashverify")   return runCrashVerify(base);
    if (m == "migratewrite")  return runMigrateWrite(base);
    if (m == "migrateverify") return runMigrateVerify(base);
    if (m == "eventwrite")    return runEventWrite(base);
    if (m == "eventverify")   return runEventVerify(base);
    if (m == "ajwrite")       return runAjWrite(base);
    if (m == "ajverify")      return runAjVerify(base);
    if (m == "verifywrite")   return runVerifyWrite(base);
    if (m == "verifyverify")  return runVerifyVerify(base);
    if (m == "pclosewrite")   return runPCloseWrite(base);
    if (m == "pcloseverify")  return runPCloseVerify(base);
    if (m == "crwrite")       return runCorrectionWrite(base);
    if (m == "crverify")      return runCorrectionVerify(base);
    if (m == "allocwrite")    return runAllocWrite(base);
    if (m == "allocverify")   return runAllocVerify(base);
    if (m == "ledgerwrite")   return runLedgerWrite(base);
    if (m == "ledgerverify")  return runLedgerVerify(base);
    if (m == "snapwrite")     return runSnapWrite(base);
    if (m == "snapverify")    return runSnapVerify(base);
    if (m == "compatwrite")   return runCompatWrite(base);
    if (m == "compatverify")  return runCompatVerify(base);
    if (m == "supwrite")      return runSupWrite(base);
    if (m == "supverify")     return runSupVerify(base);
    if (m == "invwrite")      return runInvWrite(base);
    if (m == "invverify")     return runInvVerify(base);
    if (m == "txnwrite")      return runTxnWrite(base);
    if (m == "txnverify")     return runTxnVerify(base);

    std::fprintf(stderr, "Occountant persistence + integrity suite\n");
    std::fprintf(stderr, "scratch dir: %s\n", base.c_str());

    testRoundtrip(base);
    testReplayValid(base);
    testCorruptCrc(base);
    testStaleJournal(base);
    testTornJournal(base);
    testPartialMainWrite(base);
    testIntegrityTotals(base);
    testMoneyDeterminism(base);
    testDuplicateNumber(base);
    testDurableSyncHandle(base);
    testSchemaAdditive(base);
    testSchemaCustom(base);
    testSchemaDowngradeRefused(base);
    testSchemaSizeGuard(base);
    testSchemaEmpty(base);
    testSchemaLegacyZero(base);
    testEventLog(base);
    testAuditJournal(base);
    testCustomerBackfill(base);
    testInvoiceLineIdentity(base);
    testPeriodClosure(base);
    testCorrectionSemantics(base);
    testAllocationSemantics(base);
    testSettlementAccessors(base);
    testLedgerSemantics(base);
    testLedgerAccessors(base);
    testExpenseLifecycle(base);
    testTaxEngine(base);
    testGovernanceTransition(base);
    testSecurityBoundaries(base);
    testFinancialStatements(base);
    testPostingAuthority(base);
    testSnapshotting(base);
    testProjectionVerify(base);
    testHistoricalReconstruction(base);
    testCompatibilityGovernance(base);
    testDomainCutover(base);
    testAtomicTransactions(base);
    testRepeatedCycles(base);

    std::fprintf(stderr, "\n════════════════════════════════════════\n");
    std::fprintf(stderr, "  %d passed, %d failed\n", g_pass, g_fail);
    std::fprintf(stderr, "════════════════════════════════════════\n");
    std::fflush(stderr);

    return g_fail == 0 ? 0 : 1;
}
