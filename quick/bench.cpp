#include "bench.h"

#include <QQmlApplicationEngine>
#include <QObject>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <cstdio>

#include "InvoiceListModel.h"
#include "CustomerListModel.h"
#include "InvoicesViewModel.h"
#include "InvoiceEditorViewModel.h"
#include "storage/StorageService.h"
#include "core/Invoice.h"
#include "core/Customer.h"
#include "core/Money.h"
#include "core/IsoDate.h"

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#endif

static void out(const QString& s) { std::fputs(s.toUtf8().constData(), stderr); std::fputc('\n', stderr); std::fflush(stderr); }

static std::size_t rssKB()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return pmc.WorkingSetSize / 1024;
#endif
    return 0;
}

void benchSeed(int n)
{
    auto& s = StorageService::instance();
    if (!s.isInitialized()) return;
    const int numCust = qMax(1, n / 5);

    for (int i = 0; i < numCust; ++i) {
        const QByteArray nm = ("Customer " + QString::number(i)).toUtf8();
        CustomerData d{};
        d.name = nm.constData(); d.email = "c@example.com"; d.phone = "0550000000";
        d.taxNumber = ""; d.balance = Money(); d.isDeleted = false;
        Customer c(d);
        s.customers().save(c);   // nm outlives this construct+save (same scope)
    }

    const auto iss = IsoDate::fromString("2026-01-01").value();
    const auto due = IsoDate::fromString("2026-02-01").value();
    for (int i = 0; i < n; ++i) {
        const QByteArray num = ("INV-" + QString::number(10000 + i)).toUtf8();
        const double amt = 100.0 + (i % 50);
        InvoiceData d{};
        d.invoiceNumber = num.constData();
        d.customerId = static_cast<uint32_t>(i % numCust);
        d.issueDate = iss; d.dueDate = due;
        d.subtotal  = Money::fromDouble(amt);
        d.taxAmount = Money::fromDouble(amt * 0.1);
        d.total     = Money::fromDouble(amt * 1.1);
        d.status = (i % 4 == 0) ? INVOICE_DRAFT
                 : (i % 4 == 1) ? INVOICE_POSTED
                 : (i % 4 == 2) ? INVOICE_PAID : INVOICE_OVERDUE;
        d.isDeleted = false;
        Invoice inv(d);
        s.invoices().save(inv);
    }
}

int runBenchmark(QQmlApplicationEngine& engine,
                 InvoiceListModel& invoiceModel,
                 CustomerListModel& customerModel,
                 InvoicesViewModel& invoicesVm,
                 InvoiceEditorViewModel& invoiceEditor,
                 double startupInitMs,
                 double startupEngineMs)
{
    auto& s = StorageService::instance();
    QElapsedTimer t;
    auto ms = [&t](auto&& fn) { t.restart(); fn(); return t.nsecsElapsed() / 1.0e6; };

    const int nInv  = static_cast<int>(s.invoices().loadAll().size());
    const int nCust = static_cast<int>(s.customers().loadAll().size());

    const double tLoadInv   = ms([&]{ const auto v = s.invoices().loadAll();  (void)v; });
    const double tLoadCust  = ms([&]{ const auto v = s.customers().loadAll(); (void)v; });
    const double tAgg       = ms([&]{ const auto a = s.computeCustomerAggregates(); (void)a; });
    const double tRefreshI  = ms([&]{ invoiceModel.refresh(); });
    const double tRefreshC  = ms([&]{ customerModel.refresh(); });
    const double tSummary   = ms([&]{ invoicesVm.refresh(); });
    const double tSearch    = ms([&]{ invoicesVm.setSearchText(QStringLiteral("INV-19999")); });
    invoicesVm.setSearchText(QString());
    const double tFilter    = ms([&]{ invoicesVm.setStatusFilter(QStringLiteral("Paid")); });
    invoicesVm.setStatusFilter(QStringLiteral("All"));
    const double tBeginNew  = ms([&]{ invoiceEditor.beginNew(); });

    // Repeated editor open/close — watch for memory growth / delegate churn.
    const int cycles = qEnvironmentVariableIsSet("ACCT_BENCH_CYCLES")
        ? qEnvironmentVariable("ACCT_BENCH_CYCLES").toInt() : 100;
    const std::size_t rssBefore = rssKB();
    QObject* ed = engine.rootObjects().isEmpty() ? nullptr
                : engine.rootObjects().first()->findChild<QObject*>("invoiceEditorRoot");
    const bool noBegin = qEnvironmentVariableIsSet("ACCT_BENCH_NOBEGIN"); // isolate open/close
    const double tCycles = ms([&]{
        for (int i = 0; i < cycles && ed; ++i) {
            if (!noBegin) invoiceEditor.beginNew();
            QMetaObject::invokeMethod(ed, "open");
            QMetaObject::invokeMethod(ed, "close");
            QCoreApplication::processEvents();
        }
    });
    const std::size_t rssAfter = rssKB();
    // Distinguish a TRUE leak from (a) deferred JS GC and (b) undrained deleteLater:
    // QObjects destroyed by the Repeater on model reset are deleted via DeferredDelete
    // events that a tight loop never drains — a real app's event loop does.
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
    engine.collectGarbage();
    engine.trimComponentCache();
    QCoreApplication::processEvents();
    const std::size_t rssAfterGc = rssKB();

    auto row = [](const QString& name, double v, const QString& unit = QStringLiteral("ms")) {
        out(QStringLiteral("  %1 %2 %3")
            .arg(name, -34).arg(v, 9, 'f', 2).arg(unit));
    };
    out(QStringLiteral("== BENCHMARK  invoices=%1  customers=%2 ==").arg(nInv).arg(nCust));
    row("startup: init + model.refresh",  startupInitMs);
    row("startup: engine.load (QML)",     startupEngineMs);
    row("invoices loadAll",               tLoadInv);
    row("customers loadAll",              tLoadCust);
    row("computeCustomerAggregates",      tAgg);
    row("InvoiceListModel.refresh",       tRefreshI);
    row("CustomerListModel.refresh",      tRefreshC);
    row("InvoicesViewModel.refresh(summary)", tSummary);
    row("search filter (1 keystroke)",    tSearch);
    row("status filter switch",           tFilter);
    row("editor beginNew",                tBeginNew);
    row(QStringLiteral("%1x editor open/close").arg(cycles), tCycles);
    out(QStringLiteral("  %1 %2 MB  (delta %3 MB over %4 cycles)")
        .arg(QStringLiteral("working set after cycles"), -34)
        .arg(rssAfter / 1024.0, 9, 'f', 1)
        .arg((double(rssAfter) - double(rssBefore)) / 1024.0, 0, 'f', 1)
        .arg(cycles));
    out(QStringLiteral("  %1 %2 MB  (reclaimed %3 MB by drain+GC = was deferred, not leaked)")
        .arg(QStringLiteral("working set after deleteLater drain+GC"), -34)
        .arg(rssAfterGc / 1024.0, 9, 'f', 1)
        .arg((double(rssAfter) - double(rssAfterGc)) / 1024.0, 0, 'f', 1));
    return 0;
}
