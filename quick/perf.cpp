#include "perf.h"

#include "storage/EventLog.h"
#include "storage/AuditJournal.h"
#include "storage/CompatibilityManifest.h"
#include "storage/CustomerRepository.h"
#include "storage/SupplierRepository.h"
#include "storage/InvoiceRepository.h"
#include "storage/InvoiceLineRepository.h"
#include "core/Customer.h"
#include "core/Invoice.h"
#include "core/InvoiceLine.h"
#include "core/Money.h"
#include "core/IsoDate.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <io.h>
#  define PERF_FSYNC(fd) _commit(fd)
#else
#  include <unistd.h>
#  define PERF_FSYNC(fd) ::fsync(fd)
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Evidence harness. Generates a deterministic valid event log, then measures the
// O(history) engine operations, decomposing cost into the two classes: in-memory
// LOG-FOLDS (cheap) vs fsync-per-record PROJECTION MATERIALIZATION (the real cost).
// ─────────────────────────────────────────────────────────────────────────────
namespace {

using clk = std::chrono::steady_clock;
template <class F> double timeMs(F&& fn)
{
    const auto t0 = clk::now(); fn(); const auto t1 = clk::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed) : s(seed) {}
    uint64_t next() {
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    bool chance(int pct) { return static_cast<int>(next() % 100) < pct; }
};

std::size_t rssKB()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) return pmc.WorkingSetSize / 1024;
#endif
    return 0;
}
uint64_t fileSize(const std::string& p) { std::error_code ec; auto n = std::filesystem::file_size(p, ec); return ec ? 0 : n; }
void rm(const std::string& p) { std::error_code ec; std::filesystem::remove(p, ec); }

// ── Stats ────────────────────────────────────────────────────────────────────
struct Stat { double median = 0, mn = 0, mx = 0, sd = 0; int n = 0; };
Stat stats(std::vector<double> v)
{
    Stat s; s.n = static_cast<int>(v.size());
    if (v.empty()) return s;
    std::sort(v.begin(), v.end());
    s.mn = v.front(); s.mx = v.back(); s.median = v[v.size() / 2];
    double mean = 0; for (double x : v) mean += x; mean /= v.size();
    double var = 0; for (double x : v) var += (x - mean) * (x - mean); var /= v.size();
    s.sd = std::sqrt(var);
    return s;
}
void row(const char* name, const Stat& s, const char* unit = "ms")
{
    std::fprintf(stderr, "  %-30s  median %10.3f  [min %10.3f  max %10.3f  sd %8.3f] %-3s  n=%d\n",
                 name, s.median, s.mn, s.mx, s.sd, unit, s.n);
    std::fflush(stderr);
}
Stat repeat(int runs, const std::function<double()>& one)
{
    std::vector<double> v; v.reserve(runs);
    for (int i = 0; i < runs; ++i) v.push_back(one());
    return stats(v);
}

// ── Single-fsync latency microbench (to attribute the projection-materialization cost) ──
double fsyncLatencyMs(const std::string& dir, int m)
{
    const std::string p = dir + "/.fsync_probe";
    FILE* f = std::fopen(p.c_str(), "wb");
    if (!f) return 0;
    char b[64] = {0};
    const auto t0 = clk::now();
    for (int i = 0; i < m; ++i) { std::fwrite(b, 1, 64, f); std::fflush(f); PERF_FSYNC(fileno(f)); }
    const auto t1 = clk::now();
    std::fclose(f); rm(p);
    return std::chrono::duration<double, std::milli>(t1 - t0).count() / m;
}

// ── Event payload builders (valid on-disk layouts; ids assigned in creation order) ──
std::vector<char> govPayload()
{
    const GovernanceVersions v = compat::current();
    std::vector<char> b(14);
    std::memcpy(b.data() + 0,  &v.schema,         2);
    std::memcpy(b.data() + 2,  &v.replay,         2);
    std::memcpy(b.data() + 4,  &v.postingPolicy,  2);
    std::memcpy(b.data() + 6,  &v.statement,      2);
    std::memcpy(b.data() + 8,  &v.snapshot,       2);
    std::memcpy(b.data() + 10, &v.eventLogFormat, 2);
    std::memcpy(b.data() + 12, &v.engineBuild,    2);
    return b;
}
std::vector<char> accountPayload(uint32_t aid, uint8_t type, const char* name)
{
    std::vector<char> b(36, 0);
    std::memcpy(b.data(), &aid, 4); b[4] = static_cast<char>(type);
    std::strncpy(b.data() + 5, name, 30);
    return b;
}
std::vector<char> customerPayload(uint32_t id)
{
    Customer c; c.setId(id);
    const std::string nm = "Cust " + std::to_string(id);
    c.setName(nm.c_str()); c.setEmail("c@example.com"); c.setPhone("0550000000");
    std::vector<char> b(CUSTOMER_RECORD_SIZE);
    c.serialize(b.data());
    return b;
}
std::vector<char> invoicePayload(uint32_t invId, uint32_t custId, uint32_t lineId, int64_t totalCents)
{
    Invoice inv; inv.setId(invId);
    const std::string num = "INV-" + std::to_string(invId);
    inv.setInvoiceNumber(num.c_str()); inv.setCustomerId(custId);
    inv.setIssueDate(IsoDate::fromString("2026-06-01").value());
    inv.setDueDate(IsoDate::fromString("2026-07-01").value());
    inv.setStatus(INVOICE_POSTED);
    inv.setSubtotal(Money::fromCents(totalCents));
    inv.setTaxAmount(Money::fromCents(0));
    inv.setTotal(Money::fromCents(totalCents));

    std::vector<char> b(INVOICE_RECORD_SIZE + 2 + INVOICE_LINE_RECORD_SIZE);
    inv.serialize(b.data());
    const uint16_t cnt = 1; std::memcpy(b.data() + INVOICE_RECORD_SIZE, &cnt, 2);
    InvoiceLineData d; d.description = "Line"; d.quantityMilliunits = 1000;
    d.unitPrice = Money::fromCents(totalCents); d.taxRatePermille = 0;
    InvoiceLine ln(d); ln.recompute(); ln.setId(lineId); ln.setInvoiceId(invId);
    ln.serialize(b.data() + INVOICE_RECORD_SIZE + 2);
    return b;
}
std::vector<char> journalPayload(uint32_t entryId, uint32_t acctDr, uint32_t acctCr, int64_t amt)
{
    std::vector<char> b(20 + 24, 0);
    std::memcpy(b.data() + 0, &entryId, 4);
    const uint32_t none = 0xFFFFFFFFu; std::memcpy(b.data() + 4, &none, 4);
    std::memcpy(b.data() + 8, "2026-06-15", 10);
    const uint16_t n = 2; std::memcpy(b.data() + 18, &n, 2);
    char* p = b.data() + 20;
    std::memcpy(p + 0, &acctDr, 4); std::memcpy(p + 4, &amt, 8); p += 12;
    const int64_t neg = -amt; std::memcpy(p + 0, &acctCr, 4); std::memcpy(p + 4, &neg, 8);
    return b;
}
std::vector<char> paymentPayload(uint32_t pid, uint32_t cid, int64_t amt)
{
    std::vector<char> b(26, 0);
    std::memcpy(b.data() + 0, &pid, 4); std::memcpy(b.data() + 4, &cid, 4);
    std::memcpy(b.data() + 8, &amt, 8); std::memcpy(b.data() + 16, "2026-06-20", 10);
    return b;
}

// ── Paths ────────────────────────────────────────────────────────────────────
struct Paths {
    std::string log, cursor, snap, manifest, cust, sup, inv, line;
    explicit Paths(const std::string& d)
        : log(d + "/audit.log"), cursor(d + "/audit.cursor"), snap(d + "/audit.cursor.ledgersnap"),
          manifest(d + "/compat.manifest"), cust(d + "/customers.dat"), sup(d + "/suppliers.dat"),
          inv(d + "/invoices.dat"), line(d + "/invoicelines.dat") {}
};

// ── Generator ────────────────────────────────────────────────────────────────
uint64_t generate(const std::string& dir, uint64_t target, uint64_t seed)
{
    Paths P(dir);
    for (const std::string& p : { P.log, P.cursor, P.snap, P.manifest, P.cust, P.sup, P.inv, P.line }) {
        rm(p); rm(p + ".journal");
    }
    Rng r(seed);
    EventLog log(P.log);

    std::vector<EventLog::FrameSpec> batch;
    constexpr std::size_t BATCH = 10000;
    uint64_t emitted = 0, batches = 0;
    int64_t ts = 1;
    auto flush = [&] { if (!batch.empty()) { log.appendAtomic(batch); batch.clear(); ++batches; } };
    auto push = [&](uint16_t type, std::vector<char> payload) {
        batch.push_back({ type, 1, ts++, std::move(payload) });
        ++emitted;
        if (batch.size() >= BATCH) flush();
    };

    push(15, govPayload());   // EngineVersionStamp (genesis)
    const char* names[5] = { "Accounts Receivable", "Revenue", "Cash", "Expense", "Equity" };
    const uint8_t types[5] = { 1, 4, 1, 5, 3 };   // Asset, Income, Asset, Expense, Equity
    for (uint32_t a = 0; a < 5; ++a) push(13, accountPayload(a, types[a], names[a]));

    uint32_t custId = 0, invId = 0, lineId = 0, entryId = 0, payId = 0;
    while (emitted < target) {
        push(1, customerPayload(custId++));
        for (int k = 0; k < 2 && emitted < target; ++k) {
            const int64_t total = static_cast<int64_t>(100 + (r.next() % 5000)) * 100;
            push(3, invoicePayload(invId++, custId - 1, lineId++, total));
            if (emitted >= target) break;
            push(14, journalPayload(entryId++, 0, 1, total));   // balanced Dr AR / Cr Revenue
        }
        if (r.chance(20) && emitted < target)
            push(10, paymentPayload(payId++, 0, static_cast<int64_t>(r.next() % 10000) * 100));
    }
    flush();
    return emitted;
}

// ── Measurement ──────────────────────────────────────────────────────────────
int measure(const std::string& dir, int runs)
{
    Paths P(dir);
    const std::string scratch = dir + "/.perfscratch";
    std::error_code ec; std::filesystem::create_directories(scratch, ec);

    uint64_t N = 0;
    { EventLog log(P.log); N = log.lastSeq(); }
    if (N == 0) { std::fprintf(stderr, "perf: empty log at %s — run gen first\n", P.log.c_str()); return 1; }

    // Fold ops are cheap (in-memory); projection-materialization ops are per-record durable
    // writes (journal + fsync), ~constant cost/record → 1 run suffices (variance ~0) and they
    // are gated by size (at 1M a single rebuild is ~hours; the unit cost + linearity from the
    // smaller sizes extrapolates it). Log-folds are measured at every size, including 1M.
    const int foldRuns = (N <= 150000) ? std::max(3, std::min(runs, 7)) : 3;
    const int matRuns  = 1;
    const bool doReplay      = (N <= 150000);   // measure at 10k/100k; extrapolate 1M
    const bool doReconstruct = (N <= 150000);
    const bool doVerify      = (N <= 15000) && !std::getenv("ACCT_PERF_NOVERIFY");   // ~2x replay

    std::fprintf(stderr, "\n════════ PERF  events=%llu  foldRuns=%d  matRuns=%d ════════\n",
                 static_cast<unsigned long long>(N), foldRuns, matRuns);

    // Live repos + the journal under test (this construction IS the startup index build).
    CustomerRepository c(P.cust); SupplierRepository s(P.sup);
    InvoiceRepository  ir(P.inv); InvoiceLineRepository lr(P.line);

    // ── Startup / index build (fold): construct AuditJournal (rebuild*Index, 5 log scans) ──
    row("startup: index build (ctor)", repeat(foldRuns, [&] {
        return timeMs([&] { AuditJournal aj(P.log, P.cursor, &c, &ir, &lr, &s); });
    }));

    AuditJournal aj(P.log, P.cursor, &c, &ir, &lr, &s);

    // ── Replay / projection materialization (rebuildProjections: clear + replay + project) ──
    const std::size_t rssBefore = rssKB();
    double replayMs = 0;
    if (doReplay) {
        const Stat rp = repeat(matRuns, [&] { return timeMs([&] { aj.rebuildProjections(); }); });
        row("replay: full projection rebuild", rp);
        replayMs = rp.median;
    } else {
        std::fprintf(stderr, "  %-30s  (skipped at >150k — extrapolated from the unit cost below;\n"
                             "  %-30s   fold/query ops below need no projection, so it is NOT materialized here)\n", "replay: full projection rebuild", "");
    }
    const std::size_t rssAfter = rssKB();

    // ── Fold ops (in-memory, cheap) ──
    row("ledger: trialBalanceTotal",  repeat(foldRuns, [&] { return timeMs([&] { volatile int64_t x = aj.trialBalanceTotal(); (void)x; }); }));
    row("ledger: balanceAt(acct, N)", repeat(foldRuns, [&] { return timeMs([&] { volatile int64_t x = aj.balanceAt(0, N); (void)x; }); }));
    row("statement: incomeStatementAt", repeat(foldRuns, [&] { return timeMs([&] { auto v = aj.incomeStatementAt(N); (void)v; }); }));
    row("statement: balanceSheetAt",    repeat(foldRuns, [&] { return timeMs([&] { auto v = aj.balanceSheetAt(N); (void)v; }); }));
    row("snapshot: create (writeLedgerSnapshot)", repeat(foldRuns, [&] { return timeMs([&] { aj.writeLedgerSnapshot(aj.lastSeq()); }); }));
    row("snapshot: restore (balanceUsingSnapshot)", repeat(foldRuns, [&] { return timeMs([&] { volatile int64_t x = aj.balanceUsingSnapshot(0, N); (void)x; }); }));

    // ── Historical reconstruction (materialize, customer-only) at the midpoint seq ──
    if (doReconstruct) {
        row("reconstruct: customers @ N/2", repeat(matRuns, [&] {
            CustomerRepository sc(scratch + "/rc.dat");
            return timeMs([&] { volatile uint32_t h = aj.reconstructInto(sc, N / 2); (void)h; });
        }));
    } else {
        std::fprintf(stderr, "  %-30s  (skipped at >150k — materialize, extrapolated)\n", "reconstruct: customers @ N/2");
    }

    // ── Compatibility verification (materialize: verifyAll ×2 + snapshot). ~2x replay. ──
    if (doVerify) {
        row("verify: validateCompatibility (full model)", repeat(matRuns, [&] {
            CustomerRepository    vc(scratch + "/vc.dat"); SupplierRepository    vs(scratch + "/vs.dat");
            InvoiceRepository     vi(scratch + "/vi.dat"); InvoiceLineRepository vl(scratch + "/vl.dat");
            return timeMs([&] { auto r = aj.validateCompatibility(vc, vs, vi, vl); (void)r; });
        }));
    } else {
        std::fprintf(stderr, "  %-30s  (skipped >15k — ~2x replay; ratio confirmed at 10k)\n", "verify: validateCompatibility");
    }

    // ── Profiling: fsync latency + per-record materialization unit cost ──
    const double fs = fsyncLatencyMs(dir, 200);
    // Entity records projected ≈ customers + invoices + lines (0 if not materialized at this size).
    const uint64_t entityRecords = c.count() + ir.count() + lr.count();
    std::fprintf(stderr, "\n  profiling:\n");
    std::fprintf(stderr, "    single fsync latency          %.4f ms  (200-sample mean)\n", fs);
    if (doReplay && entityRecords > 0) {
        std::fprintf(stderr, "    projected entity records      %llu  (customers+invoices+lines)\n", (unsigned long long)entityRecords);
        std::fprintf(stderr, "    replay per-record UNIT COST   %.4f ms/record  (= replay / records; the scaling constant)\n",
                     replayMs / double(entityRecords));
        std::fprintf(stderr, "    per-record vs single fsync    %.1fx  (journal file create+write+fsync+delete per record)\n",
                     (replayMs / double(entityRecords)) / (fs > 0 ? fs : 1));
    }

    // ── Memory + disk ──
    const uint64_t diskLog = fileSize(P.log), diskDat = fileSize(P.cust) + fileSize(P.inv) + fileSize(P.line) + fileSize(P.sup);
    const uint64_t diskSnap = fileSize(P.snap), diskTot = diskLog + diskDat + diskSnap + fileSize(P.cursor) + fileSize(P.manifest);
    std::fprintf(stderr, "\n  memory:\n");
    std::fprintf(stderr, "    RSS (engine indices + folds)  %.1f MB\n", rssAfter / 1024.0);
    std::fprintf(stderr, "    RSS bytes / event             %.1f\n", double(rssAfter) * 1024.0 / double(N));
    (void)rssBefore;
    std::fprintf(stderr, "  disk:\n");
    std::fprintf(stderr, "    audit.log                     %.2f MB   (%.1f bytes/event)\n", diskLog / 1048576.0, double(diskLog) / double(N));
    std::fprintf(stderr, "    projections (*.dat)           %.2f MB\n", diskDat / 1048576.0);
    std::fprintf(stderr, "    snapshot                      %.2f MB\n", diskSnap / 1048576.0);
    std::fprintf(stderr, "    total on disk                 %.2f MB   (%.1f bytes/event)\n", diskTot / 1048576.0, double(diskTot) / double(N));

    // ── Determinism canary: the generated book must verify + balance to 0. ──
    const bool trial0 = (aj.trialBalanceTotal() == 0);
    std::fprintf(stderr, "\n  canary: trialBalance==0 = %s  (deterministic data)\n", trial0 ? "yes" : "NO");
    std::fflush(stderr);
    return trial0 ? 0 : 1;
}

} // namespace

int runPerf(const QString& mode, const QString& dataDir)
{
    const std::string dir = dataDir.toStdString();
    std::error_code ec; std::filesystem::create_directories(dir, ec);
    const std::string m = mode.toStdString();

    uint64_t seed = 0x9E3779B97F4A7C15ull;   // fixed default seed (deterministic)
    if (const char* s = std::getenv("ACCT_PERF_SEED")) seed = std::strtoull(s, nullptr, 0);

    if (m.rfind("gen:", 0) == 0) {
        const uint64_t n = std::strtoull(m.c_str() + 4, nullptr, 10);
        const auto t0 = clk::now();
        const uint64_t emitted = generate(dir, n, seed);
        const double ms = std::chrono::duration<double, std::milli>(clk::now() - t0).count();
        std::fprintf(stderr, "perf gen: %llu events in %.0f ms  (%.0f events/sec)  seed=0x%llx\n",
                     (unsigned long long)emitted, ms, emitted / (ms / 1000.0), (unsigned long long)seed);
        return 0;
    }
    if (m.rfind("measure:", 0) == 0) {
        const int runs = std::atoi(m.c_str() + 8);
        return measure(dir, runs < 1 ? 5 : runs);
    }
    std::fprintf(stderr, "perf: unknown mode '%s' (use gen:<N> or measure:<runs>)\n", m.c_str());
    return 2;
}
