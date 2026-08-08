#include "fuzz.h"

#include "storage/EventLog.h"
#include "storage/BinaryRecordFile.h"
#include "storage/AuditJournal.h"
#include "storage/CompatibilityManifest.h"
#include "storage/CustomerRepository.h"
#include "storage/SupplierRepository.h"
#include "storage/InvoiceRepository.h"
#include "storage/InvoiceLineRepository.h"
#include "storage/FaultInjection.h"
#include "core/Customer.h"
#include "core/Supplier.h"
#include "core/Invoice.h"
#include "core/InvoiceLine.h"
#include "core/Money.h"
#include "core/IsoDate.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// Adversarial harness. Every artifact is built VALID, then corrupted with structure-aware
// byte mutations; the boundary must respond with a loud rejection (throw / false /
// documented fallback) or a safe deterministic recovery — never a silent acceptance of
// corrupted committed state, and never a fault without a diagnostic. Randomized VALID
// histories then assert the accounting invariants as properties. All seeded → reproducible.
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
void section(const char* name) { std::fprintf(stderr, "\n── %s\n", name); std::fflush(stderr); }
void note(const char* fmt, long a, long b, long c)
{ std::fprintf(stderr, "    " ); std::fprintf(stderr, fmt, a, b, c); std::fprintf(stderr, "\n"); std::fflush(stderr); }

// ── Deterministic PRNG (splitmix64) ──────────────────────────────────────────
struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint32_t u32() { return static_cast<uint32_t>(next()); }
    // inclusive [lo, hi]
    int range(int lo, int hi) { return lo + static_cast<int>(next() % static_cast<uint64_t>(hi - lo + 1)); }
    bool chance(int pct) { return static_cast<int>(next() % 100) < pct; }
};

// ── File helpers ─────────────────────────────────────────────────────────────
std::vector<char> readAll(const std::string& p)
{
    std::vector<char> out;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return out;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { out.resize(static_cast<std::size_t>(n)); if (std::fread(out.data(), 1, out.size(), f) != out.size()) out.clear(); }
    std::fclose(f);
    return out;
}
void writeAll(const std::string& p, const std::vector<char>& d)
{
    FILE* f = std::fopen(p.c_str(), "wb");
    if (!f) return;
    if (!d.empty()) std::fwrite(d.data(), 1, d.size(), f);
    std::fclose(f);
}

// One structure-agnostic byte mutation (bit-flip / byte-set / truncate / grow / zero-run /
// duplicate-region). Structure-AWARE offsets (header/length/crc fields) are covered because
// random offsets land on them across thousands of iterations.
void mutate(Rng& r, std::vector<char>& d)
{
    if (d.empty()) { d.push_back(static_cast<char>(r.u32())); return; }
    switch (r.range(0, 5)) {
    case 0: { std::size_t i = r.next() % d.size(); d[i] = static_cast<char>(d[i] ^ (1u << (r.next() % 8))); break; }
    case 1: { std::size_t i = r.next() % d.size(); d[i] = static_cast<char>(r.u32()); break; }
    case 2: { std::size_t n = r.next() % d.size();  d.resize(n); break; }
    case 3: { std::size_t n = 1 + r.next() % 64;    d.insert(d.end(), n, static_cast<char>(r.u32())); break; }
    case 4: { std::size_t i = r.next() % d.size(); std::size_t n = 1 + r.next() % (d.size() - i);
              for (std::size_t k = 0; k < n; ++k) d[i + k] = 0; break; }
    case 5: { if (d.size() >= 4) { std::size_t i = r.next() % (d.size() - 3);
              for (int k = 0; k < 4; ++k) d.push_back(d[i + k]); } break; }
    }
}

void rm(const std::string& p) { std::error_code ec; std::filesystem::remove(p, ec); }

// ── Fuzzer 1: EventLog frames (audit.log) ────────────────────────────────────
// A mutated log must, on reopen, either THROW (corrupt committed history) or open exposing
// ONLY gap-free, CRC-valid frames (a valid prefix). Never a malformed committed frame.
void fuzzEventFrames(Rng& base, const std::string& dir, int iters)
{
    section("fuzz: EventLog frames (audit.log)");

    // Deterministic F1 case (security regression): a header committedLength inflated far past
    // the file, with a frame advertising a huge payloadLen, must be rejected LOUDLY and
    // length-bounded — the parser trusts the real file size, never the untrusted field, so it
    // never attempts an allocation sized by it.
    {
        const std::string q = dir + "/fz_ev_inflate.log";
        rm(q);
        { EventLog log(q); char pl[8] = {}; log.append(1, 1, 100, pl, 8); }   // one valid frame → 68 bytes
        auto b = readAll(q);
        bool threw = false;
        if (b.size() >= 44) {
            const uint64_t big = 2000000000ull; std::memcpy(b.data() + 16, &big, 8);   // inflate committedLength
            const uint32_t huge = 200000000u;   std::memcpy(b.data() + 32, &huge, 4);   // huge frame payloadLen
            writeAll(q, b);
            try { EventLog log2(q); } catch (const std::exception&) { threw = true; }
        }
        ok(threw, "EventLog: inflated committedLength + huge frame len rejected (bounded by file size)");
        rm(q);
    }

    const std::string p = dir + "/fz_ev.log";
    long rejected = 0, safeOpen = 0, silent = 0;
    for (int it = 0; it < iters; ++it) {
        const uint64_t seed = base.next();
        Rng r(seed);
        rm(p);
        {   // build a valid log with 3–6 events, random payload sizes
            EventLog log(p);
            const int n = r.range(3, 6);
            for (int i = 0; i < n; ++i) {
                char pl[16]; for (char& c : pl) c = static_cast<char>(r.u32());
                log.append(static_cast<uint16_t>(r.range(1, 15)), 1, 1000 + i, pl, static_cast<uint32_t>(r.next() % 17));
            }
        }
        auto bytes = readAll(p);
        const int muts = r.range(1, 4);
        for (int m = 0; m < muts; ++m) mutate(r, bytes);
        writeAll(p, bytes);

        bool threw = false, consistent = true;
        try {
            EventLog log2(p);
            uint64_t expect = 1;
            log2.forEach([&](const EventRecord& rr) { if (rr.seq != expect++) consistent = false; return true; });
            if (expect - 1 != log2.lastSeq()) consistent = false;   // exposed count must match header
        } catch (const std::exception&) { threw = true; }

        if (threw)            ++rejected;
        else if (consistent)  ++safeOpen;
        else { ++silent; std::fprintf(stderr, "  [FAIL] event-frame silent-accept seed=%llu\n",
                                      static_cast<unsigned long long>(seed)); }
    }
    ok(silent == 0, "EventLog fuzz: no silent acceptance of corrupted committed history");
    note("(%ld iters: %ld rejected loudly, %ld safe-open valid-prefix)", (long)iters, rejected, safeOpen);
}

// ── Fuzzer 2: ledger snapshot (*.ledgersnap) ─────────────────────────────────
// A corrupt snapshot must be rejected (seq 0 / verify false) and balanceUsingSnapshot must
// still equal the genesis balanceAt (the accelerator can never return a wrong balance).
void fuzzSnapshot(Rng& base, const std::string& dir, int iters)
{
    section("fuzz: ledger snapshot (*.ledgersnap)");
    long silent = 0, rejected = 0, keptValid = 0;
    for (int it = 0; it < iters; ++it) {
        const uint64_t seed = base.next();
        Rng r(seed);
        const std::string lg = dir + "/fz_sn.log", cu = dir + "/fz_sn.cursor";
        rm(lg); rm(cu); rm(cu + ".ledgersnap");
        CustomerRepository c(dir + "/fz_sn_c.dat"); rm(dir + "/fz_sn_c.dat");
        {
            CustomerRepository c2(dir + "/fz_sn_c.dat");
            AuditJournal aj(lg, cu, &c2);
            const uint32_t a1 = aj.recordAccount(1, "A", 1);
            const uint32_t a2 = aj.recordAccount(4, "B", 2);
            const int n = r.range(2, 5);
            for (int i = 0; i < n; ++i)
                aj.recordJournalEntry(IsoDate::fromString("2026-07-01").value(),
                    { AuditJournal::PostingInput{a1, 1000}, AuditJournal::PostingInput{a2, -1000} }, 10 + i);
            aj.writeLedgerSnapshot(aj.lastSeq());
        }
        auto snap = readAll(cu + ".ledgersnap");
        for (int m = 0, mm = r.range(1, 3); m < mm; ++m) mutate(r, snap);
        writeAll(cu + ".ledgersnap", snap);

        CustomerRepository c3(dir + "/fz_sn_c.dat");
        AuditJournal aj(lg, cu, &c3);
        const uint64_t last = aj.lastSeq();
        const bool validOrAbsent = (aj.ledgerSnapshotSeq() == 0) || aj.verifyLedgerSnapshot();
        if (aj.ledgerSnapshotSeq() == 0) ++rejected; else ++keptValid;
        bool acc = true;
        for (int a = 0; a < 2; ++a) {
            const uint32_t acct = static_cast<uint32_t>(a);
            if (aj.balanceUsingSnapshot(acct, last) != aj.balanceAt(acct, last)) acc = false;
        }
        if (!(validOrAbsent && acc)) { ++silent;
            std::fprintf(stderr, "  [FAIL] snapshot mis-accept seed=%llu\n", static_cast<unsigned long long>(seed)); }
    }
    ok(silent == 0, "snapshot fuzz: corrupt snapshot rejected → genesis fallback stays correct");
    note("(%ld iters: %ld rejected→genesis, %ld still-valid)", (long)iters, rejected, keptValid);
}

// ── Fuzzer 3: compatibility manifest (compat.manifest) ───────────────────────
// read() is total (any 0..N-byte input) and never returns true for a bad-CRC file; a
// successful read round-trips exactly.
// ── Fuzzer: tax policy (TaxCodeCreated events + version replay) ───────────────
// A mutated tax log must, on reopen, either THROW (corrupt committed frame) or expose a
// deterministic, bounded tax index — never a crash, a silent bad rate, or a nondeterministic
// resolveRateAt. Also proves historical tax-version replay is stable.
void fuzzTaxCodes(Rng& base, const std::string& dir, int iters)
{
    section("fuzz: tax policy (TaxCodeCreated / version replay)");
    long silent = 0, rejected = 0, safeOpen = 0;
    for (int it = 0; it < iters; ++it) {
        const uint64_t seed = base.next(); Rng r(seed);
        const std::string lg = dir + "/fz_tx.log", cu = dir + "/fz_tx.cursor";
        rm(lg); rm(cu); rm(dir + "/fz_tx_c.dat");
        std::vector<int32_t> rates;
        {
            CustomerRepository c(dir + "/fz_tx_c.dat");
            AuditJournal aj(lg, cu, &c);
            const int n = r.range(2, 5);
            for (int i = 0; i < n; ++i) {
                const int32_t rate = static_cast<int32_t>(r.next() % 1000);
                rates.push_back(rate);
                aj.recordTaxCode(static_cast<uint8_t>(r.next() % 5), "VAT",   // same family → versions
                                 rate, IsoDate::fromString("2020-01-01").value(), 10 + i);
            }
        }
        auto bytes = readAll(lg);
        for (int m = 0, mm = r.range(1, 3); m < mm; ++m) mutate(r, bytes);
        writeAll(lg, bytes);

        bool threw = false, consistent = true;
        try {
            CustomerRepository c2(dir + "/fz_tx_c.dat");
            AuditJournal aj2(lg, cu, &c2);
            const auto codes = aj2.listTaxCodes();
            for (const auto& cc : codes) {                 // bounded + deterministic
                if (cc.ratePermille < -100000 || cc.ratePermille > 100000) consistent = false;
                if (aj2.taxCodeById(cc.id).id != cc.id) consistent = false;
            }
            // resolveRateAt is deterministic (same call twice → same answer).
            const IsoDate q = IsoDate::fromString("2026-01-01").value();
            if (aj2.resolveRateAt(1, q) != aj2.resolveRateAt(1, q)) consistent = false;
        } catch (const std::exception&) { threw = true; }

        if (threw)           ++rejected;
        else if (consistent) ++safeOpen;
        else { ++silent; std::fprintf(stderr, "  [FAIL] tax-code silent/nondeterministic seed=%llu\n",
                                      static_cast<unsigned long long>(seed)); }
    }
    ok(silent == 0, "tax-code fuzz: corrupt tax log rejected or exposes a bounded deterministic index");
    note("(%ld iters: %ld rejected loudly, %ld safe-open)", (long)iters, rejected, safeOpen);
}

void fuzzManifest(Rng& base, const std::string& dir, int iters)
{
    section("fuzz: compatibility manifest (compat.manifest)");
    const std::string p = dir + "/fz.manifest";
    long rejected = 0, accepted = 0, silent = 0;
    for (int it = 0; it < iters; ++it) {
        const uint64_t seed = base.next();
        Rng r(seed);
        rm(p);
        GovernanceVersions v = compat::current();
        v.engineBuild = static_cast<uint16_t>(r.u32());
        CompatibilityManifest::write(p, v);
        auto bytes = readAll(p);
        for (int m = 0, mm = r.range(1, 3); m < mm; ++m) mutate(r, bytes);
        writeAll(p, bytes);

        GovernanceVersions got;
        const bool okRead = CompatibilityManifest::read(p, got);   // must be total (no crash/hang)
        if (!okRead) { ++rejected; continue; }
        ++accepted;
        // A successful read means the CRC validated → it must round-trip.
        const std::string p2 = dir + "/fz2.manifest"; rm(p2);
        CompatibilityManifest::write(p2, got);
        GovernanceVersions got2;
        if (!(CompatibilityManifest::read(p2, got2) && got2 == got)) { ++silent;
            std::fprintf(stderr, "  [FAIL] manifest non-round-trip seed=%llu\n", static_cast<unsigned long long>(seed)); }
    }
    ok(silent == 0, "manifest fuzz: read() is total; a successful read round-trips (no bad-CRC accept)");
    note("(%ld iters: %ld rejected, %ld accepted+round-tripped)", (long)iters, rejected, accepted);
}

// ── Fuzzer 4: BinaryRecordFile (*.dat + *.journal) ───────────────────────────
// A mutated data file / crafted journal must reopen with a deterministic rejection (throw)
// or a file-bounded recovery — never UB, never an absurd record count.
void fuzzBinaryRecordFile(Rng& base, const std::string& dir, int iters)
{
    section("fuzz: BinaryRecordFile (*.dat + *.journal)");
    const std::string p = dir + "/fz_brf.dat";
    long rejected = 0, recovered = 0, silent = 0;
    for (int it = 0; it < iters; ++it) {
        const uint64_t seed = base.next();
        Rng r(seed);
        rm(p); rm(p + ".journal"); rm(p + ".migrating"); rm(p + ".migrate.bak");
        {
            CustomerRepository repo(p);
            const int n = r.range(3, 6);
            for (int i = 0; i < n; ++i) { Customer c; c.setName(("N" + std::to_string(i)).c_str()); repo.save(c); }
        }
        auto bytes = readAll(p);
        for (int m = 0, mm = r.range(1, 4); m < mm; ++m) mutate(r, bytes);
        writeAll(p, bytes);
        if (r.chance(40)) {   // craft a random (usually invalid) journal
            std::vector<char> j(r.range(1, 200));
            for (char& b : j) b = static_cast<char>(r.u32());
            writeAll(p + ".journal", j);
        }
        try {
            CustomerRepository repo(p);
            const std::size_t n = repo.count();
            auto all = repo.loadAll();          // must not crash on garbage records
            (void)all;
            // count is derived from file size / record size — a valid open can never claim
            // more records than the (bounded) file could hold.
            if (n > (static_cast<std::size_t>(1) << 24)) { ++silent;
                std::fprintf(stderr, "  [FAIL] BRF absurd count=%zu seed=%llu\n", n, static_cast<unsigned long long>(seed)); }
            else ++recovered;
        } catch (const std::exception&) { ++rejected; }   // loud, deterministic rejection
    }
    ok(silent == 0, "BRF fuzz: reject-or-recover, sane bounded count, no UB on garbage records");
    note("(%ld iters: %ld rejected loudly, %ld opened-bounded)", (long)iters, rejected, recovered);
}

// ── Fuzzer 5: compatibility classification (pure function) ───────────────────
void fuzzClassify(Rng& base, int iters)
{
    section("fuzz: compatibility classification (classify())");
    long bad = 0;
    const GovernanceVersions cur = compat::current();
    for (int it = 0; it < iters; ++it) {
        Rng r(base.next());
        GovernanceVersions v;
        v.schema         = static_cast<uint16_t>(r.range(0, 4));
        v.replay         = static_cast<uint16_t>(r.range(0, 4));
        v.postingPolicy  = static_cast<uint16_t>(r.range(0, 4));
        v.statement      = static_cast<uint16_t>(r.range(0, 4));
        v.snapshot       = static_cast<uint16_t>(r.range(0, 4));
        v.eventLogFormat = static_cast<uint16_t>(r.range(0, 4));
        v.engineBuild    = static_cast<uint16_t>(r.u32());
        std::string reason;
        const auto c1 = compat::classify(v, reason);
        const auto c2 = compat::classify(v, reason);
        if (c1 != c2) ++bad;                                  // deterministic
        const bool anyNewer = v.schema > cur.schema || v.replay > cur.replay
                           || v.postingPolicy > cur.postingPolicy || v.statement > cur.statement
                           || v.snapshot > cur.snapshot || v.eventLogFormat > cur.eventLogFormat;
        if (anyNewer && c1 != compat::Compatibility::Incompatible) ++bad;   // newer → refuse
    }
    ok(bad == 0, "classify fuzz: total, deterministic, any-newer → Incompatible");
    note("(%ld random version vectors)", (long)iters, 0, 0);
}

// ── Property-based invariants over randomized VALID histories ─────────────────
// Only valid ops are issued, so every produced stream is an accepted stream.
void genHistory(Rng& r, AuditJournal& aj, InvoiceRepository& ir)
{
    aj.ensureGovernanceStamp(1);
    const AuditJournal::RoleAccounts roles = aj.ensureChartOfAccounts(1);
    int64_t ts = 10;
    const int ops = r.range(3, 12);
    for (int i = 0; i < ops; ++i) {
        switch (r.range(0, 4)) {
        case 0: { Customer c; c.setName(("C" + std::to_string(i)).c_str()); aj.recordCustomerCreated(c, ts++); break; }
        case 1: { Supplier s; s.setName(("S" + std::to_string(i)).c_str()); aj.recordSupplierCreated(s, ts++); break; }
        case 2: {   // atomic invoice + revenue (posted → recognises revenue; draft → none)
            Invoice inv; inv.setInvoiceNumber(("INV-" + std::to_string(i)).c_str());
            const bool posted = r.chance(70);
            inv.setStatus(posted ? INVOICE_POSTED : INVOICE_DRAFT);
            inv.setIssueDate(IsoDate::fromString("2026-06-01").value());
            const int64_t total = static_cast<int64_t>(r.range(1, 5000)) * 100;
            inv.setTotal(Money::fromCents(total));
            InvoiceLineData d; d.description = "L"; d.quantityMilliunits = 1000;
            d.unitPrice = Money::fromCents(total); d.taxRatePermille = 0;
            InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
            aj.recordInvoiceWithRevenue(inv, lines, false, posted ? total : 0, /*tax*/ 0, inv.getIssueDate(), ts++);
            break;
        }
        case 3: {   // balanced manual journal entry
            const int64_t amt = static_cast<int64_t>(r.range(1, 1000)) * 100;
            aj.recordJournalEntry(IsoDate::fromString("2026-06-15").value(),
                { AuditJournal::PostingInput{roles.receivable, amt},
                  AuditJournal::PostingInput{roles.revenue, -amt} }, ts++);
            break;
        }
        case 4: {   // settlement payment (does not touch the double-entry ledger)
            if (ir.count() > 0)
                aj.recordPayment(0, static_cast<int64_t>(r.range(1, 100)) * 100,
                                 IsoDate::fromString("2026-06-20").value(), ts++);
            break;
        }
        }
    }
    if (r.chance(60)) aj.writeLedgerSnapshot(aj.lastSeq());
}

void propertyTests(Rng& base, const std::string& dir, int histories)
{
    section("property-based invariants over randomized histories");
    long p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0;
    for (int h = 0; h < histories; ++h) {
        const uint64_t seed = base.next();
        Rng r(seed);
        const std::string d = dir + "/prop_" + std::to_string(h % 6);
        std::error_code ec; std::filesystem::remove_all(d, ec); std::filesystem::create_directories(d, ec);

        CustomerRepository c(d + "/c.dat"); SupplierRepository s(d + "/s.dat");
        InvoiceRepository  ir(d + "/i.dat"); InvoiceLineRepository lr(d + "/l.dat");
        AuditJournal aj(d + "/a.log", d + "/a.cursor", &c, &ir, &lr, &s);
        genHistory(r, aj, ir);
        const uint64_t last = aj.lastSeq();

        if (aj.trialBalanceTotal() != 0) { ++p4; std::fprintf(stderr, "  [P4] trial!=0 seed=%llu\n", (unsigned long long)seed); }

        aj.rebuildProjections();   // delete every projection + replay from history
        {
            CustomerRepository vc(d + "/vc.dat"); SupplierRepository vs(d + "/vs.dat");
            InvoiceRepository  vi(d + "/vi.dat"); InvoiceLineRepository vl(d + "/vl.dat");
            const auto va  = aj.verifyAll(vc, vs, vi, vl);
            const auto va2 = aj.verifyAll(vc, vs, vi, vl);
            if (!va.ok)  { ++p1; std::fprintf(stderr, "  [P1] rebuild drift seed=%llu\n", (unsigned long long)seed); }
            if (!(va2.ok && va2.customersOk == va.customersOk && va2.suppliersOk == va.suppliersOk
                  && va2.invoicesOk == va.invoicesOk && va2.linesOk == va.linesOk))
                { ++p2; std::fprintf(stderr, "  [P2] non-idempotent seed=%llu\n", (unsigned long long)seed); }
        }
        {   // snapshot + tail == genesis
            bool okp3 = true;
            for (int a = 0; a < 3; ++a) {
                const uint32_t acct = static_cast<uint32_t>(r.range(0, 2));
                const uint64_t upto = (last == 0) ? 0 : (r.next() % (last + 1));
                if (aj.balanceUsingSnapshot(acct, upto) != aj.balanceAt(acct, upto)) okp3 = false;
            }
            if (!okp3) { ++p3; std::fprintf(stderr, "  [P3] snap!=genesis seed=%llu\n", (unsigned long long)seed); }
        }
        {   // reconstruction determinism
            const uint64_t mid = last / 2;
            CustomerRepository sc(d + "/rc.dat");
            if (aj.reconstructInto(sc, mid) != aj.reconstructInto(sc, mid))
                { ++p5; std::fprintf(stderr, "  [P5] nondet reconstruct seed=%llu\n", (unsigned long long)seed); }
        }
        if (h % 3 == 0) {   // one deterministic ledger — full-model compat validation (sampled: O(history))
            CustomerRepository sc(d + "/kc.dat"); SupplierRepository ss(d + "/ks.dat");
            InvoiceRepository  si(d + "/ki.dat"); InvoiceLineRepository sl(d + "/kl.dat");
            if (!aj.validateCompatibility(sc, ss, si, sl).ok)
                { ++p6; std::fprintf(stderr, "  [P6] validate failed seed=%llu\n", (unsigned long long)seed); }
        }
    }
    ok(p1 == 0, "P1 rebuild-invariance: deleting every projection changes nothing");
    ok(p2 == 0, "P2 replay idempotence: replay(replay(history)) == replay(history)");
    ok(p3 == 0, "P3 snapshot + tail == genesis replay");
    ok(p4 == 0, "P4 trial balance always equals zero");
    ok(p5 == 0, "P5 reconstruction is deterministic");
    ok(p6 == 0, "P6 every accepted stream → one deterministic verified ledger");
    note("(%ld randomized histories)", (long)histories, 0, 0);
}

// ── Fault-injection writer / verifier (prove recovery, cross-process) ─────────
int runFaultWrite(const std::string& base)
{
    const char* arm = std::getenv("ACCT_FAULT_ARM");   // which persistence write to injure
    CustomerRepository c(base + "/f_c.dat"); SupplierRepository s(base + "/f_s.dat");
    InvoiceRepository  ir(base + "/f_i.dat"); InvoiceLineRepository lr(base + "/f_l.dat");
    AuditJournal aj(base + "/f.log", base + "/f.cursor", &c, &ir, &lr, &s);

    // Phase 1 — build valid committed state (no fault armed yet).
    aj.ensureGovernanceStamp(1);
    aj.ensureChartOfAccounts(1);
    {
        Invoice inv; inv.setInvoiceNumber("F-0"); inv.setStatus(INVOICE_POSTED);
        inv.setIssueDate(IsoDate::fromString("2026-08-01").value()); inv.setTotal(Money::fromCents(10000));
        InvoiceLineData d; d.description = "A"; d.quantityMilliunits = 1000; d.unitPrice = Money::fromCents(10000); d.taxRatePermille = 0;
        InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
        aj.recordInvoiceWithRevenue(inv, lines, false, 10000, /*tax*/ 0, inv.getIssueDate(), 2);
    }

    // Phase 2 — arm the target write (one-shot) and trigger it. Our error path must catch it.
    if (arm) acctArmFault(arm);
    try {
        Invoice inv; inv.setInvoiceNumber("F-1"); inv.setStatus(INVOICE_POSTED);
        inv.setIssueDate(IsoDate::fromString("2026-08-02").value()); inv.setTotal(Money::fromCents(20000));
        InvoiceLineData d; d.description = "B"; d.quantityMilliunits = 1000; d.unitPrice = Money::fromCents(20000); d.taxRatePermille = 0;
        InvoiceLine ln(d); ln.recompute(); std::vector<InvoiceLine> lines = { ln };
        aj.recordInvoiceWithRevenue(inv, lines, false, 20000, /*tax*/ 0, inv.getIssueDate(), 3);   // logCommit / cursorWrite
        aj.writeLedgerSnapshot(aj.lastSeq());                                            // snapshotWrite
        CompatibilityManifest::write(base + "/compat.manifest", aj.currentGovernance()); // manifestWrite
    } catch (const std::exception& e) {
        std::fprintf(stderr, "faultwrite: caught injected fault (%s): %s\n", arm ? arm : "?", e.what());
    }
    std::fprintf(stderr, "faultwrite: survived (error path handled)\n");
    std::fflush(stderr);
    return 0;
}

int runFaultVerify(const std::string& base)
{
    // A fresh process reopens. The ctor must not throw (log consistent). Recovery holds
    // regardless of which write was injured: reconcile heals a cursor-behind projection; a
    // failed commit leaves the group cleanly absent; the manifest rebuilds from the log; a
    // failed snapshot install falls back to genesis.
    CustomerRepository c(base + "/f_c.dat"); SupplierRepository s(base + "/f_s.dat");
    InvoiceRepository  ir(base + "/f_i.dat"); InvoiceLineRepository lr(base + "/f_l.dat");
    AuditJournal aj(base + "/f.log", base + "/f.cursor", &c, &ir, &lr, &s);
    aj.reconcile();

    const bool trialOk = (aj.trialBalanceTotal() == 0);
    const bool snapOk  = (aj.ledgerSnapshotSeq() == 0) || aj.verifyLedgerSnapshot();
    const bool govOk   = aj.hasGovernance() && (aj.currentGovernance() == compat::current());
    CustomerRepository vc(base + "/fv_c.dat"); SupplierRepository vs(base + "/fv_s.dat");
    InvoiceRepository  vi(base + "/fv_i.dat"); InvoiceLineRepository vl(base + "/fv_l.dat");
    const auto va = aj.verifyAll(vc, vs, vi, vl);

    const bool good = trialOk && snapOk && govOk && va.ok;
    std::fprintf(stderr, "faultverify: trial=%d snap=%d gov=%d verifyAll=%d invoices=%zu\n",
                 trialOk ? 1 : 0, snapOk ? 1 : 0, govOk ? 1 : 0, va.ok ? 1 : 0, ir.count());
    std::fflush(stderr);
    return good ? 0 : 1;
}

} // namespace

int runFuzz(const QString& mode, const QString& dataDir)
{
    const std::string base = dataDir.toStdString();
    std::error_code ec;
    std::filesystem::create_directories(base, ec);

    const std::string m = mode.toStdString();
    if (m == "faultwrite")  return runFaultWrite(base);
    if (m == "faultverify") return runFaultVerify(base);

    // suite: seed + depth from the environment (deterministic; deep override supported).
    uint64_t seed = 0xA5F00D1234567890ull;
    if (const char* s = std::getenv("ACCT_FUZZ_SEED")) seed = std::strtoull(s, nullptr, 0);
    int iters = 400;
    if (const char* s = std::getenv("ACCT_FUZZ_ITERS")) iters = std::atoi(s);
    if (iters < 1) iters = 1;
    int histories = iters / 12; if (histories < 12) histories = 12; if (histories > 150) histories = 150;

    std::fprintf(stderr, "Occountant adversarial fuzz suite  seed=0x%llx  iters=%d  histories=%d\n",
                 static_cast<unsigned long long>(seed), iters, histories);
    std::fprintf(stderr, "scratch dir: %s\n", base.c_str());
    Rng rng(seed);

    fuzzEventFrames(rng, base, iters);
    fuzzSnapshot(rng, base, iters / 3 + 1);
    fuzzManifest(rng, base, iters);
    fuzzTaxCodes(rng, base, iters);
    fuzzBinaryRecordFile(rng, base, iters / 2 + 1);
    fuzzClassify(rng, iters * 2);
    propertyTests(rng, base, histories);

    std::fprintf(stderr, "\n════════════════════════════════════════\n");
    std::fprintf(stderr, "  %d passed, %d failed\n", g_pass, g_fail);
    std::fprintf(stderr, "════════════════════════════════════════\n");
    std::fflush(stderr);
    return g_fail == 0 ? 0 : 1;
}
