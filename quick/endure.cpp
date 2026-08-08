#include "endure.h"

#include "InvoiceEditorViewModel.h"
#include "CustomerEditorViewModel.h"
#include "InvoicesViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "LocaleController.h"
#include "diagnostics.h"
#include "storage/StorageService.h"
#include "core/Customer.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QString>

#include <cstddef>
#include <cstdio>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#endif

namespace {

void out(const QString& s) { std::fputs(s.toUtf8().constData(), stderr); std::fputc('\n', stderr); std::fflush(stderr); }

std::size_t rssKB()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1024;
#endif
    return 0;
}

// Drain queued deletions so memory samples reflect reclaimed objects, not the
// backlog of DeferredDelete events (the Phase-1 "leak" measurement artifact).
void drain()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

int warnTotal()
{
    return diag::counters().warnings.load() + diag::counters().criticals.load();
}

} // namespace

int runEndurance(InvoiceEditorViewModel& ie,
                 CustomerEditorViewModel& ce,
                 InvoicesViewModel& ivm,
                 LocaleController& lo,
                 int cycles)
{
    auto& s = StorageService::instance();

    // Ensure a customer id exists for invoice commits (FK is just an int — the
    // commit doesn't dereference it, but we keep one real customer for realism).
    int custId = -1;
    {
        auto all = s.customers().loadAll();
        if (!all.empty()) custId = static_cast<int>(all.front().getId());
        else { Customer c; c.setName("Endurance Co"); custId = static_cast<int>(s.customers().save(c)); }
    }
    ie.refreshCustomerOptions();

    drain();
    const std::size_t rssStart = rssKB();
    const int warnStart = warnTotal();

    const char* langs[] = { "en", "fr", "ar" };
    int saved = 0, failed = 0;
    QElapsedTimer t; t.start();

    for (int i = 0; i < cycles; ++i) {
        // 1. Open editor → fill → save (the dominant churn path).
        ie.beginNew();
        ie.setCustomerId(custId);
        ie.lines()->setCell(0, "description",   "Item");
        ie.lines()->setCell(0, "qtyText",       "2");
        ie.lines()->setCell(0, "unitPriceText", "19.99");
        if (ie.commit()) ++saved; else ++failed;

        // 2. Filter / search churn (proxy re-evaluation).
        ivm.setSearchText("Item");
        ivm.setStatusFilter("all");
        ivm.setSearchText("");

        // 3. Runtime language switching (retranslate path — must not churn objects).
        lo.setLanguage(langs[i % 3]);

        // 4. Periodic customer add (refreshes the cached options too).
        if (i % 50 == 0) {
            ce.beginNew();
            ce.setName(QString("Cust %1").arg(i));
            ce.commit();
            ie.refreshCustomerOptions();
        }

        if ((i + 1) % 250 == 0) {
            drain();
            out(QString("  cycle %1/%2: rss=%3 MB | saves=%4 | %5")
                    .arg(i + 1).arg(cycles)
                    .arg(rssKB() / 1024.0, 0, 'f', 1)
                    .arg(saved)
                    .arg(diag::summary()));
        }
    }

    lo.setLanguage("en");
    drain();

    const std::size_t rssEnd = rssKB();
    const int warnEnd = warnTotal();
    const double secs = t.elapsed() / 1000.0;
    const double growthMB = (static_cast<double>(rssEnd) - static_cast<double>(rssStart)) / 1024.0;

    out("");
    out(QString("ENDURANCE: %1 cycles in %2s — %3 saves, %4 failed")
            .arg(cycles).arg(secs, 0, 'f', 1).arg(saved).arg(failed));
    out(QString("  memory: %1 MB → %2 MB  (Δ %3 MB after DeferredDelete drain)")
            .arg(rssStart / 1024.0, 0, 'f', 1).arg(rssEnd / 1024.0, 0, 'f', 1)
            .arg(growthMB, 0, 'f', 1));
    out(QString("  warning drift: %1 → %2  (Δ %3)").arg(warnStart).arg(warnEnd).arg(warnEnd - warnStart));
    out(QString("  final counters: %1").arg(diag::summary()));

    // Pass criteria: every save succeeded, ZERO warning drift (no leak of warnings
    // over a long session), and bounded memory growth after draining.
    const bool pass = (failed == 0) && (warnEnd == warnStart) && (growthMB < 64.0);
    out(QString("  verdict: %1  (failed==0, drift==0, growth<64MB)").arg(pass ? "PASS" : "FAIL"));
    return pass ? 0 : 1;
}
