#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QFont>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QSettings>
#include <QQuickWindow>
#include <QImage>

#include "InvoiceListModel.h"
#include "AppController.h"
#include "InvoicesViewModel.h"
#include "InvoiceEditorViewModel.h"
#include "InvoiceDraftLinesModel.h"
#include "LocaleController.h"
#include "storage/StorageService.h"
#include "storage/CompatibilityManifest.h"
#include "CustomerListModel.h"
#include "CustomersViewModel.h"
#include "CustomerEditorViewModel.h"
#include "SupplierListModel.h"
#include "SuppliersViewModel.h"
#include "SupplierEditorViewModel.h"
#include "PaymentListModel.h"
#include "PaymentsViewModel.h"
#include "PaymentEditorViewModel.h"
#include "PaymentAllocationViewModel.h"
#include "AccountsListModel.h"
#include "AccountsViewModel.h"
#include "TrialBalanceModel.h"
#include "LedgerExplorerViewModel.h"
#include "ExpenseListModel.h"
#include "ExpensesViewModel.h"
#include "ExpenseEditorViewModel.h"
#include "TaxSummaryViewModel.h"
#include "TaxCodeEditorViewModel.h"
#include "DiagnosticsViewModel.h"
#include "SettingsViewModel.h"
#include "PeriodCloseViewModel.h"
#include "OnboardingViewModel.h"
#include "ExportViewModel.h"
#include "BackupViewModel.h"
#include "EarlyAccessViewModel.h"
#include "SupportCenterViewModel.h"
#include "PlatformController.h"
#include "app/AppInfo.h"
#include "app/Logging.h"
#include "app/LicenseManager.h"
#include "app/BackupScheduler.h"
#include "app/UpdateManager.h"
#include "app/CrashReporter.h"
#include "app/StartupDiagnostics.h"
#include "app/BuildInfo.h"
#include "app/SupportBundle.h"
#include "app/SupportId.h"
#include "app/Signature.h"
#include "c2test.h"
#include <QSysInfo>
#include "itest.h"
#include "accept.h"
#include "hostile_accept.h"
#include "pilot_checks.h"
#include "early_access.h"
#include "bench.h"
#include "ptest.h"
#include "fuzz.h"
#include "perf.h"
#include "endure.h"
#include "a11y.h"
#include "logging.h"
#include "diagnostics.h"
#include <QElapsedTimer>
#include <cstdio>

int main(int argc, char* argv[])
{
    // Install the runtime-warning capture FIRST, before any Qt object can emit, so
    // platform/engine warnings during startup are categorized and counted too.
    diag::install();

    // The a11y audit needs the accessibility bridge active; it reads QT_ACCESSIBILITY
    // at QGuiApplication construction, so set it before.
    if (qEnvironmentVariableIsSet("ACCT_A11Y"))
        qputenv("QT_ACCESSIBILITY", "1");

    QGuiApplication qgapp(argc, argv);

    qgapp.setApplicationName(QString::fromLatin1(appinfo::kProduct));   // "Occountant"
    qgapp.setOrganizationName(QString::fromLatin1(appinfo::kOrg));      // path-clean org for QSettings/QStandardPaths
    qgapp.setApplicationVersion(appinfo::version());

    // Font fallback chains. QML's font value type only accepts a single family
    // string, so we register substitutions here: when a primary family isn't
    // installed (Inter / IBM Plex Sans Arabic aren't bundled yet), Qt falls back
    // through these. Segoe UI covers both Latin and Arabic on Windows today; when
    // the brand fonts are bundled later, the substitutions simply stop triggering.
    QFont::insertSubstitutions("Inter", {"Segoe UI", "Arial", "sans-serif"});
    QFont::insertSubstitutions("IBM Plex Sans Arabic", {"Segoe UI", "Tahoma", "Arial"});

    QFont f = qgapp.font();
    f.setFamilies({"Inter", "Segoe UI"});
    qgapp.setFont(f);

    // Portable mode: a marker file beside the executable redirects ALL app state — books, license,
    // updates, logs, and preferences — to <exeDir>/Data, so Occountant runs from a USB stick and
    // leaves nothing in %LOCALAPPDATA% or the registry. The ACCT_DATA_DIR test override still wins.
    // This is bootstrap wiring ABOVE the engine; the accounting engine is unaware of it.
    const QString exeDir = QCoreApplication::applicationDirPath();
    const bool portable = QFile::exists(exeDir + "/Occountant.portable")
                       || QFile::exists(exeDir + "/portable.ini");
    const QString portableRoot = exeDir + "/Data";
    if (portable) {
        // Keep preferences self-contained too (QSettings defaults to the registry on Windows).
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, portableRoot + "/config");
    }

    // ACCT_DATA_DIR overrides the storage location — lets the screenshot harness
    // run against an isolated, deterministically-seeded dataset without touching
    // the user's real books.
    const QString dataPath = qEnvironmentVariableIsSet("ACCT_DATA_DIR")
        ? qEnvironmentVariable("ACCT_DATA_DIR")
        : portable ? portableRoot
        : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    // Persistence / integrity test harness (deterministic, isolated). Runs against
    // its own scratch files under dataPath and never opens the real StorageService,
    // so it cannot touch a user's books. Exits before any UI is created.
    if (qEnvironmentVariableIsSet("ACCT_PTEST")) {
        return runPersistenceTests(qEnvironmentVariable("ACCT_PTEST"), dataPath);
    }

    // Build-provenance dump (ACCT_BUILDINFO=<path>): write the embedded, deterministic
    // BuildInfo.json and exit. tools/release.sh lays this into the install tree + release manifest.
    if (qEnvironmentVariableIsSet("ACCT_BUILDINFO")) {
        QFile f(qEnvironmentVariable("ACCT_BUILDINFO"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return 2;
        f.write(buildinfo::json());
        f.close();
        return 0;
    }

    // Offline detached-signing helper (ACCT_SIGN=<path>): print the Ed25519 hex payload signature
    // the updater verifies, to stdout. The release pipeline uses this to sign an update payload with
    // no network. Requires a signing-enabled build (ACCT_DEV_SIGNING); a release binary has no
    // secret and prints an empty signature. (Authenticode exe/installer signing is a separate
    // OS-level step — see tools/sign-authenticode.sh.)
    if (qEnvironmentVariableIsSet("ACCT_SIGN")) {
        QFile f(qEnvironmentVariable("ACCT_SIGN"));
        if (!f.open(QIODevice::ReadOnly)) return 2;
        const QByteArray sigHex = sig::signDetached(f.readAll());
        f.close();
        std::fwrite(sigHex.constData(), 1, size_t(sigHex.size()), stdout);
        std::fputc('\n', stdout);
        return 0;
    }

    // Adversarial robustness / fuzz / fault-injection harness (deterministic, isolated).
    // Runs against its own scratch files under dataPath; exits before any UI is created.
    if (qEnvironmentVariableIsSet("ACCT_FUZZ")) {
        return runFuzz(qEnvironmentVariable("ACCT_FUZZ"), dataPath);
    }

    // Commercial-infrastructure (C2) regression harness (deterministic, network-free, isolated).
    // Exercises licensing / updater / backup scheduler / crash reporter / logging / startup
    // diagnostics entirely in a scratch dir; exits before any UI is created.
    if (qEnvironmentVariableIsSet("ACCT_C2TEST")) {
        return runC2Tests(qEnvironmentVariable("ACCT_C2TEST"));
    }

    // Performance & scalability harness (deterministic, isolated). Generates synthetic
    // event histories and measures the engine at scale; exits before any UI is created.
    if (qEnvironmentVariableIsSet("ACCT_PERF")) {
        return runPerf(qEnvironmentVariable("ACCT_PERF"), dataPath);
    }

    // Diagnostics self-test: prove the warning-capture pipeline classifies, counts,
    // dedups, and excludes our own acct.* logs. Emits representative messages and
    // asserts the counters. Deterministic; exits before the engine loads.
    if (qEnvironmentVariableIsSet("ACCT_DIAGTEST")) {
        qWarning("Unable to assign [undefined] to QColor");        // binding
        qWarning("QML anchors: Possible anchor loop detected");    // layout
        qWarning("Cannot open: qrc:/missing.png");                 // resource
        qWarning("Translations will not be available");            // i18n
        qCritical("ReferenceError: foo is not defined");           // binding + crit
        qCInfo(lcStorage) << "ours — must NOT be counted as a defect";
        qWarning("duplicate-noise"); qWarning("duplicate-noise");  // dedup → counts 2, prints once
        const diag::Counters& c = diag::counters();
        const bool ok = c.binding.load() == 2     // "Unable to assign" + "ReferenceError"
                     && c.layout.load() == 1
                     && c.resource.load() == 1
                     && c.translation.load() == 1
                     && c.criticals.load() == 1
                     && c.other.load() == 2;       // both dups counted (dedup only suppresses printing)
        std::fprintf(stderr, "DIAGTEST: %s | %s\n", ok ? "PASS" : "FAIL",
                     diag::summary().toUtf8().constData());
        return ok ? 0 : 1;
    }

    // On Windows with MSYS2, the QML import modules live under <exeDir>/qml/.
    // Add it explicitly so the engine finds QtQuick.Controls.Basic, QtQuick.Layouts, etc.
    const QString qmlImportDir =
        QDir(QCoreApplication::applicationDirPath()).filePath("qml");
    if (QDir(qmlImportDir).exists())
        qputenv("QML_IMPORT_PATH", qmlImportDir.toLocal8Bit());

    QQuickStyle::setStyle("Basic");

    QElapsedTimer startupTimer;
    startupTimer.start();

    // ── Commercial (C2) platform layer: machine-global config dir + production logging ──
    // License + update staging live here (independent of the per-books data dir). Nothing below
    // touches the accounting engine.
    const QString configPath = qEnvironmentVariableIsSet("ACCT_CONFIG_DIR")
        ? qEnvironmentVariable("ACCT_CONFIG_DIR")
        : portable ? portableRoot + "/config"
        : QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configPath);
    prodlog::init({ dataPath + "/logs" });
    prodlog::info("startup", QString("Occountant %1 starting").arg(appinfo::fullVersion()));

    // Apply any complete staged UPDATE before the store (and the rest of the app) opens; clean up
    // an interrupted staging. Never touches the data directory. Mirrors the restore stage-and-swap.
    switch (UpdateManager::applyPendingAtStartup(configPath + "/updates", appinfo::versionCode())) {
    case UpdateManager::ApplyResult::Applied:   qCInfo(lcStartup).noquote() << "applied a staged update"; break;
    case UpdateManager::ApplyResult::Recovered: qCWarning(lcStorage).noquote() << "cleaned an interrupted staged update"; break;
    default: break;
    }

    // Apply a user-staged restore BEFORE the store opens. BackupViewModel::restore() copies a
    // chosen backup into <dataPath>/.pending-restore/ and sets QSettings restore/pending; the live
    // files are locked while the app runs, so the swap can only happen here, at a cold start. This
    // keeps restore crash-consistent: either the staged copy fully replaces the data files, or the
    // original set is untouched.
    {
        QSettings s;
        const QString pending = s.value(QStringLiteral("restore/pending")).toString();
        const QString stage = dataPath + "/.pending-restore";
        if (!pending.isEmpty() && QDir(stage).exists()) {
            bool ok = true;
            // Copy each staged file to a temp beside the target, then atomically rename over it.
            // This avoids the "removed the live file, then the copy failed (disk full)" total-loss
            // window: the live file is only replaced once its full replacement is on disk.
            for (const QFileInfo& fi : QDir(stage).entryInfoList(QDir::Files)) {
                const QString target = dataPath + "/" + fi.fileName();
                const QString tmp    = target + ".restoring";
                QFile::remove(tmp);
                if (!QFile::copy(fi.absoluteFilePath(), tmp)) { ok = false; break; }
                QFile::remove(target);
                if (!QFile::rename(tmp, target)) { QFile::remove(tmp); ok = false; break; }
            }
            if (ok) {
                QDir(stage).removeRecursively();
                s.remove(QStringLiteral("restore/pending"));
                qCInfo(lcRecovery).noquote() << "restored data set from backup" << pending;
            } else {
                qCWarning(lcStorage).noquote()
                    << "restore incomplete — staged copy left in place, retrying next start";
            }
        }
    }

    if (!StorageService::instance().initialize(dataPath.toStdString())) {
        // Historical-compatibility refusal: the books are NEWER than this build (a
        // downgrade) or below a migration floor. Never operate on data we cannot safely
        // interpret — refuse loudly and exit, rather than run degraded and risk silently
        // reinterpreting history. (Mirrors BinaryRecordFile refuse-newer / apply() refusal.)
        if (StorageService::instance().refusedIncompatible()) {
            qCCritical(lcIntegrity).noquote()
                << "REFUSING TO OPEN — "
                << QString::fromStdString(StorageService::instance().lastInitError());
            return 3;
        }
        qCWarning(lcStorage) << "storage init failed:"
                   << QString::fromStdString(StorageService::instance().lastInitError());
        // Continue — UI will show empty state
    } else {
        qCInfo(lcStorage) << "storage opened at" << dataPath;
        // Startup recovery reporting: surface any crash-leftover journal replay.
        const bool recI = StorageService::instance().invoices().recovered();
        const bool recC = StorageService::instance().customers().recovered();
        if (recI || recC)
            qCInfo(lcRecovery).noquote()
                << "crash recovery: replayed journal(s) on open —"
                << (recI ? "invoices " : "") << (recC ? "customers" : "");
        // Schema-migration reporting: surface a forward migration of a user's books.
        const bool migI = StorageService::instance().invoices().migrated();
        const bool migC = StorageService::instance().customers().migrated();
        if (migI || migC)
            qCInfo(lcRecovery).noquote()
                << "schema migration applied on open —"
                << (migI ? "invoices " : "") << (migC ? "customers" : "");

        // Audit history reporting: event count, any reconciliation/tail recovery.
        auto& storage = StorageService::instance();
        qCInfo(lcStorage).noquote()
            << "audit history: events=" << storage.auditEventCount()
            << " reconciled=" << storage.auditReconciled()
            << " backfilled=" << storage.auditBackfilled()
            << " closedPeriods=" << storage.audit().closedPeriodCount()
            << " corrections=" << storage.audit().correctionCount()
            << " payments=" << storage.audit().paymentCount()
            << " allocations=" << storage.audit().allocationCount()
            << " accounts=" << storage.audit().accountCount()
            << " ledgerEntries=" << storage.audit().entryCount()
            << " trialBalance=" << storage.audit().trialBalanceTotal()
            << (storage.auditTornTail() ? " (discarded uncommitted tail)" : "");
        if (storage.auditBackfilled() > 0)
            qCInfo(lcRecovery).noquote()
                << "commit cutover: adopted" << storage.auditBackfilled()
                << "pre-audit record(s) into authoritative history";
        if (storage.auditReconciled() > 0)
            qCInfo(lcRecovery).noquote()
                << "audit reconcile: replayed" << storage.auditReconciled()
                << "event(s) into projections on open";

        // Opt-in deep integrity check (O(history); off by default). Rebuilds the
        // projection from authoritative history into a scratch copy and compares —
        // drift surfaces LOUDLY and is never silently ignored.  ACCT_VERIFY=1
        if (qEnvironmentVariableIsSet("ACCT_VERIFY")) {
            const auto vr = storage.verifyAuditProjection();
            if (vr.ok)
                qCInfo(lcIntegrity).noquote()
                    << "projection verified against history at seq" << vr.seq;
            else
                qCCritical(lcIntegrity).nospace().noquote()
                    << "PROJECTION DRIFT — live != history (live=" << vr.liveHash
                    << " history=" << vr.historyHash << " seq=" << vr.seq << ")";
        }

        // ── Historical compatibility & evolution governance ──────────────────────
        // One self-describing line: the version contract this build opened, and how it
        // classified the on-disk books (compatible / migration-required / incompatible).
        const GovernanceVersions gv = storage.governanceVersions();
        qCInfo(lcStartup).nospace().noquote()
            << "compat: schema=" << gv.schema << " replay=" << gv.replay
            << " postingPolicy=" << gv.postingPolicy << " statement=" << gv.statement
            << " snapshot=" << gv.snapshot << " eventLog=" << gv.eventLogFormat
            << " | " << compat::toString(storage.compatibilityStatus())
            << (storage.governanceAdopted() ? " (stamp adopted)" : "");

        // Deep replay-equivalence gate (ACCT_COMPAT_VERIFY=1): prove history reconstructs
        // to the same accounting MEANING; a mismatch is refused loudly. Headless exit code.
        if (qEnvironmentVariableIsSet("ACCT_COMPAT_VERIFY")) {
            const auto cr = storage.validateCompatibility();
            if (cr.ok) {
                qCInfo(lcIntegrity).noquote()
                    << "compatibility verified —" << QString::fromStdString(cr.detail);
                return 0;
            }
            qCCritical(lcIntegrity).noquote()
                << "COMPATIBILITY VALIDATION FAILED —" << QString::fromStdString(cr.detail);
            return 4;
        }

        // Operator compatibility report (ACCT_COMPAT_REPORT=<path>): dump the version
        // contract, migration history, and the guarantees checklist to a file, then exit.
        if (qEnvironmentVariableIsSet("ACCT_COMPAT_REPORT")) {
            const QString path = qEnvironmentVariable("ACCT_COMPAT_REPORT");
            const CompatibilityReport rep = storage.compatibilityReport(/*runValidation*/ true);
            QFile rf(path);
            if (rf.open(QIODevice::WriteOnly | QIODevice::Text)) {
                auto yn = [](bool b){ return b ? "yes" : "no"; };
                rf.write(QString(
                    "Occountant — Compatibility Report\n"
                    "versions: schema=%1 replay=%2 postingPolicy=%3 statement=%4 snapshot=%5 eventLog=%6 engineBuild=%7\n"
                    "classification: %8\n"
                    "headSeq: %9\n"
                    "migrations since genesis: %10\n")
                    .arg(rep.versions.schema).arg(rep.versions.replay).arg(rep.versions.postingPolicy)
                    .arg(rep.versions.statement).arg(rep.versions.snapshot).arg(rep.versions.eventLogFormat)
                    .arg(rep.versions.engineBuild)
                    .arg(QString::fromLatin1(compat::toString(rep.classification)))
                    .arg(rep.headSeq).arg(rep.migrationCount).toUtf8());
                rf.write(QString(
                    "guarantees satisfied:\n"
                    "  replay equivalence (genesis): %1\n"
                    "  snapshot equivalence:         %2\n"
                    "  trial balance == 0:           %3\n"
                    "  historical determinism:       %4\n"
                    "  ALL: %5\n")
                    .arg(yn(rep.replayValidated)).arg(yn(rep.snapshotValidated))
                    .arg(yn(rep.trialBalanceZero)).arg(yn(rep.historicalDeterministic))
                    .arg(yn(rep.guaranteesSatisfied())).toUtf8());
                rf.close();
            }
            return 0;
        }
    }

    // Acceptance suite (ACCT_ACCEPT=<persona>): drive a realistic business end-to-end against the
    // fresh books just opened above (company→customers→…→verification). Headless; exits with the
    // failed-assertion count. Isolated per persona by tools/acceptance.sh (one fresh dir each).
    if (qEnvironmentVariableIsSet("ACCT_ACCEPT")) {
        return runAcceptance(qEnvironmentVariable("ACCT_ACCEPT"));
    }

    // Hostile accounting-correctness audit (ACCT_HOSTILE=payments|expenses|all): drive the same
    // shipping ViewModels as the acceptance suite, but assert the CROSS-SUBSYSTEM invariants
    // (payment→ledger, three-way balance agreement, expense funding side). Exits with the count of
    // confirmed findings (deliberately non-zero on the current build). Headless; fresh ACCT_DATA_DIR.
    if (qEnvironmentVariableIsSet("ACCT_HOSTILE")) {
        return runHostileAudit(qEnvironmentVariable("ACCT_HOSTILE"));
    }

    // Pilot-readiness suite (ACCT_PILOT=safety|export|all): drive the real backup/restore + export
    // paths a pilot SMB relies on; exits with the failed-assertion count (0 = pilot-ready).
    if (qEnvironmentVariableIsSet("ACCT_PILOT")) {
        return runPilotChecks(qEnvironmentVariable("ACCT_PILOT"));
    }

    // Early Access / Support Center gate (ACCT_EARLY_ACCESS=1): the notice state machine, support-id
    // stability, privacy-safe bundle, and the invariant that support flows author NO accounting
    // events and leave replay-equivalence intact. Headless; isolated QSettings + data dir.
    if (qEnvironmentVariableIsSet("ACCT_EARLY_ACCESS")) {
        return runEarlyAccessTests(dataPath);
    }

    // Support bundle (ACCT_SUPPORT_BUNDLE=<path>): compose build info + logs + crash reports +
    // startup diagnostics + compatibility report into ONE zip for support — WITHOUT any accounting
    // data. Deterministic + offline; the Diagnostics screen exposes the same call for end users.
    if (qEnvironmentVariableIsSet("ACCT_SUPPORT_BUNDLE")) {
        supportbundle::Params p;
        p.outputZip   = qEnvironmentVariable("ACCT_SUPPORT_BUNDLE");
        p.dataDir     = dataPath;
        p.crashDir    = dataPath + "/crash";
        p.logDir      = dataPath + "/logs";
        p.reason      = QStringLiteral("cli");
        p.supportId   = supportid::get();
        const QString zip = supportbundle::generate(p);
        return zip.isEmpty() ? 5 : 0;
    }

    // Bulk-seed mode (before models load, so seeding doesn't pay model-refresh cost).
    if (qEnvironmentVariableIsSet("ACCT_BENCH_SEED")) {
        benchSeed(qEnvironmentVariable("ACCT_BENCH_SEED").toInt());
        return 0;
    }

    InvoiceListModel model;
    model.refresh();
    qInfo() << "AccountingQuick: loaded" << model.rowCount()
            << "invoices from" << dataPath;

    AppController app(dataPath, &model);
    InvoicesViewModel invoicesVm(&model);
    InvoiceEditorViewModel invoiceEditor;

    CustomerListModel custModel;
    custModel.refresh();
    CustomersViewModel customersVm(&custModel);
    CustomerEditorViewModel customerEditor;

    SupplierListModel supModel;
    supModel.refresh();
    SuppliersViewModel suppliersVm(&supModel);
    SupplierEditorViewModel supplierEditor;

    PaymentListModel payModel;
    payModel.refresh();
    PaymentsViewModel paymentsVm(&payModel);
    PaymentEditorViewModel paymentEditor;
    PaymentAllocationViewModel paymentAllocation;

    // ── Ledger explorer (read-only): every value derived from the ledger engine ──
    AccountsListModel acctModel;
    acctModel.refresh();
    AccountsViewModel accountsVm(&acctModel);
    TrialBalanceModel trialBalanceModel;
    trialBalanceModel.refresh();
    LedgerExplorerViewModel ledgerVm;   // owns its own LedgerEntriesModel (refreshed in ctor)

    // ── Expenses (event-authored operational entity) ──
    ExpenseListModel expModel;
    expModel.refresh();
    ExpensesViewModel expensesVm(&expModel);
    ExpenseEditorViewModel expenseEditor;

    // ── Tax (VAT/GST summary + tax-code registry) ──
    TaxSummaryViewModel taxSummaryVm;      // owns its own TaxCodeListModel (refreshed in ctor)
    TaxCodeEditorViewModel taxCodeEditor;

    // ── Settings & System (all read-only w.r.t. the engine / QSettings-backed prefs) ──
    DiagnosticsViewModel diagnosticsVm;    // read-only engine metrics + on-demand verification
    SettingsViewModel    settingsVm;       // QSettings prefs + startup-recovery status
    PeriodCloseViewModel periodVm;         // freeze/reopen accounting periods (existing engine capability)
    OnboardingViewModel  onboardingVm;     // first-run company profile wizard (settings only, no events)
    ExportViewModel      exportVm;         // CSV/PDF export of invoices + reports (deliver info out)
    BackupViewModel      backupVm;         // manual backup / verify / restore over the data folder
    EarlyAccessViewModel earlyAccessVm;    // Early Access welcome-notice state (QSettings, no events)
    SupportCenterViewModel supportVm;      // local problem reports + privacy-safe diagnostics (no events)
    settingsVm.captureStartupRecovery();   // snapshot any crash-recovery outcome for the UX

    // ── Commercial (C2) managers — all above StorageService; the engine is unaware ──
    LicenseManager  licenseMgr(configPath);
    licenseMgr.initialize();               // offline, deterministic; issues a trial on first run
    BackupScheduler backupScheduler(dataPath);
    UpdateManager   updateMgr(configPath + "/updates",
                              qEnvironmentVariable("ACCT_UPDATE_SOURCE", configPath + "/updates/source"),
                              appinfo::versionCode());
    // Track the user's chosen release channel and re-check when they switch it in Settings.
    updateMgr.setChannel(settingsVm.updateChannel());
    QObject::connect(&settingsVm, &SettingsViewModel::updateChannelChanged,
                     &updateMgr, [&updateMgr](const QString& ch){ updateMgr.setChannel(ch); updateMgr.check(); });
    updateMgr.check();                      // local, non-blocking availability check
    {
        // Arm crash reporting with the engine's (read-only) governance line + platform.
        crashreport::Context cc;
        cc.buildVersion = appinfo::fullVersion();
        cc.buildId      = appinfo::buildId();
        cc.channel      = appinfo::channel();
        cc.reportDir    = dataPath + "/crash";
        cc.platform     = QSysInfo::prettyProductName() + " / " + QSysInfo::currentCpuArchitecture();
        if (StorageService::instance().isInitialized()) {
            const GovernanceVersions g = StorageService::instance().governanceVersions();
            cc.governance = QString("schema %1 · replay %2 · posting %3 · snapshot %4")
                .arg(g.schema).arg(g.replay).arg(g.postingPolicy).arg(g.snapshot);
        }
        crashreport::install(cc);
    }
    PlatformController platformController(&licenseMgr, &backupScheduler, &updateMgr);

    const double startupInitMs = startupTimer.nsecsElapsed() / 1.0e6;  // init + all model loads

    // Declared BEFORE the engine so it outlives it (like the models/VMs above):
    // on shutdown the engine tears down while `locale` is still alive, so `i18n`
    // bindings don't re-evaluate against a destroyed object. The engine is wired in
    // via setEngine() once it exists (the ctor doesn't need it — retranslate is deferred).
    LocaleController locale;

    // A few ViewModels/models expose user-facing text through C++ tr() (combo-option labels,
    // journal DisplayRole text, diagnostic/recovery strings) that QML cannot host. On a live
    // language switch, retranslate() re-emits so QML re-reads them under the new translator —
    // the C++ analogue of QQmlApplicationEngine::retranslate() (which only refreshes QML qsTr).
    QObject::connect(&locale, &LocaleController::languageChanged, &diagnosticsVm, &DiagnosticsViewModel::retranslate);
    QObject::connect(&locale, &LocaleController::languageChanged, &settingsVm,    &SettingsViewModel::retranslate);
    QObject::connect(&locale, &LocaleController::languageChanged, &backupVm,      &BackupViewModel::retranslate);
    QObject::connect(&locale, &LocaleController::languageChanged, &expenseEditor, &ExpenseEditorViewModel::retranslate);
    QObject::connect(&locale, &LocaleController::languageChanged, &ledgerVm,      [&ledgerVm]{ ledgerVm.refresh(); });

    QElapsedTimer engineTimer;
    engineTimer.start();
    QQmlApplicationEngine engine;
    locale.setEngine(&engine);
    engine.rootContext()->setContextProperty("invoiceModel", &model);
    engine.rootContext()->setContextProperty("app", &app);
    engine.rootContext()->setContextProperty("invoicesVm", &invoicesVm);
    engine.rootContext()->setContextProperty("invoiceEditor", &invoiceEditor);
    engine.rootContext()->setContextProperty("customersVm", &customersVm);
    engine.rootContext()->setContextProperty("customerEditor", &customerEditor);
    engine.rootContext()->setContextProperty("suppliersVm", &suppliersVm);
    engine.rootContext()->setContextProperty("supplierEditor", &supplierEditor);
    engine.rootContext()->setContextProperty("paymentsVm", &paymentsVm);
    engine.rootContext()->setContextProperty("paymentEditor", &paymentEditor);
    engine.rootContext()->setContextProperty("paymentAllocation", &paymentAllocation);
    engine.rootContext()->setContextProperty("accountsVm", &accountsVm);
    engine.rootContext()->setContextProperty("trialBalanceModel", &trialBalanceModel);
    engine.rootContext()->setContextProperty("ledgerVm", &ledgerVm);
    engine.rootContext()->setContextProperty("expensesVm", &expensesVm);
    engine.rootContext()->setContextProperty("expenseEditor", &expenseEditor);
    engine.rootContext()->setContextProperty("taxSummaryVm", &taxSummaryVm);
    engine.rootContext()->setContextProperty("taxCodeEditor", &taxCodeEditor);
    engine.rootContext()->setContextProperty("diagnosticsVm", &diagnosticsVm);
    engine.rootContext()->setContextProperty("settingsVm", &settingsVm);
    engine.rootContext()->setContextProperty("periodVm", &periodVm);
    engine.rootContext()->setContextProperty("onboardingVm", &onboardingVm);
    engine.rootContext()->setContextProperty("exportVm", &exportVm);
    engine.rootContext()->setContextProperty("backupVm", &backupVm);
    engine.rootContext()->setContextProperty("earlyAccessVm", &earlyAccessVm);
    engine.rootContext()->setContextProperty("supportVm", &supportVm);
    engine.rootContext()->setContextProperty("platform", &platformController);

    // `locale` is constructed above (so it outlives the engine); it set the
    // translator + layoutDirection at construction, and setEngine() wired the engine
    // for deferred retranslate. Context property must be set before load so QML binds.
    // Exposed as `i18n`, NOT `locale`: every QML Item has a built-in `locale`
    // (QLocale) property that would shadow a `locale` context property inside any
    // Item scope (Menu, ApplicationWindow, …), silently breaking all access.
    engine.rootContext()->setContextProperty("i18n", &locale);

    engine.loadFromModule("App", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;
    const double startupEngineMs = engineTimer.nsecsElapsed() / 1.0e6;

    // Now that the engine has loaded QML, trigger retranslate to refresh all qsTr bindings
    locale.reapply();

    // Benchmark mode (ACCT_BENCH=1): measure real load/aggregate/filter/editor/memory.
    if (qEnvironmentVariableIsSet("ACCT_BENCH"))
        return runBenchmark(engine, model, custModel, invoicesVm, invoiceEditor,
                            startupInitMs, startupEngineMs);

    // Interaction-test mode (ACCT_ITEST=1): drive real QML paths, assert VM state +
    // persistence, fail on any QML runtime error. Returns the failure count as the
    // process exit code (0 = pass) for CI. Runs synchronously; never enters exec().
    if (qEnvironmentVariableIsSet("ACCT_ITEST"))
        return runInteractionTests(engine, invoiceEditor, customerEditor, supplierEditor,
                                   paymentEditor, paymentAllocation, locale) == 0 ? 0 : 1;

    // Endurance / long-session mode (ACCT_ENDURE=<cycles>): drive sustained
    // save/filter/language workflows, measuring memory growth + runtime-warning drift.
    if (qEnvironmentVariableIsSet("ACCT_ENDURE"))
        return runEndurance(invoiceEditor, customerEditor, invoicesVm, locale,
                            qEnvironmentVariable("ACCT_ENDURE").toInt());

    // Accessibility-tree audit (ACCT_A11Y=1): walk the real QAccessible tree and
    // assert interactive controls expose role + name.
    if (qEnvironmentVariableIsSet("ACCT_A11Y"))
        return runA11yAudit(engine);

    // Headless verification probe: with ACCT_PROBE=<path> the app writes its
    // derived counts to that file and quits shortly after load. Writing to a
    // file avoids all stdio buffering / log-category filtering on Windows.
    if (qEnvironmentVariableIsSet("ACCT_PROBE")) {
        const QString probePath = qEnvironmentVariable("ACCT_PROBE");
        QFile pf(probePath);
        if (pf.open(QIODevice::WriteOnly | QIODevice::Text)) {
            // Write initial state
            pf.write(QString("totalCount=%1 filteredCount=%2\n")
                         .arg(invoicesVm.totalCount())
                         .arg(invoicesVm.filteredCount()).toUtf8());

            // Switch to Arabic to prove RTL + translator path
            locale.setLanguage("ar");
            pf.write(QString("language=%1 layoutDir=%2 tr_Paid=%3 tr_plural2=%4\n")
                         .arg(locale.language())
                         .arg(static_cast<int>(QGuiApplication::layoutDirection()))
                         .arg(QCoreApplication::translate("StatusBadge", "Paid"))
                         .arg(qApp->translate("InvoicesScreen", "%n invoice(s)", nullptr, 2))
                         .toUtf8());

            // Switch back to English
            locale.setLanguage("en");

            // Geometry check: confirm the redesigned summary bar + rows actually
            // laid out with sane heights (childrenRect-based sizing is fragile).
            if (!engine.rootObjects().isEmpty()) {
                QObject* root = engine.rootObjects().first();
                auto h = [root](const char* name) -> double {
                    QObject* o = root->findChild<QObject*>(name);
                    return o ? o->property("height").toDouble() : -1.0;
                };
                QObject* lv = root->findChild<QObject*>("invoiceList");
                const double ch = lv ? lv->property("contentHeight").toDouble() : -1.0;
                const int    cnt = lv ? lv->property("count").toInt() : -1;
                pf.write(QString("summaryCardH=%1 heroCellH=%2 listContentH=%3 listCount=%4 avgRowH=%5\n")
                             .arg(h("summaryCard"))
                             .arg(h("heroCell"))
                             .arg(ch).arg(cnt)
                             .arg(cnt > 0 ? ch / cnt : -1.0)
                             .toUtf8());
            }

            // ── Dispatch B geometry probe (read-only) ────────────────────────
            // Verify CustomerEditor opens + customer screen geometry is sane.
            if (!engine.rootObjects().isEmpty()) {
                QObject* rootObj2 = engine.rootObjects().first();
                auto h2 = [rootObj2](const char* name) -> double {
                    QObject* o = rootObj2->findChild<QObject*>(name);
                    return o ? o->property("height").toDouble() : -1.0;
                };

                // Customer editor: open it, read visible+height, close.
                bool custEditorOpened = false;
                double custEditorH = -1.0;
                QObject* ced = rootObj2->findChild<QObject*>("customerEditorRoot");
                if (ced) {
                    customerEditor.beginNew();
                    QMetaObject::invokeMethod(ced, "open");
                    custEditorOpened = ced->property("visible").toBool();
                    custEditorH = ced->property("height").toDouble();
                    QMetaObject::invokeMethod(ced, "close");
                }

                // Customer list + summary bar heights
                QObject* clist = rootObj2->findChild<QObject*>("customerList");
                const bool customerListExists = (clist != nullptr);
                const double customerSummaryH = h2("customerSummary");

                pf.write(QString("custEditorOpened=%1 custEditorH=%2 customerSummaryH=%3 customerListExists=%4\n")
                             .arg(custEditorOpened ? "true" : "false")
                             .arg(custEditorH)
                             .arg(customerSummaryH)
                             .arg(customerListExists ? "true" : "false")
                             .toUtf8());

                // ── Language switcher runtime verification ────────────────────
                // Evaluate i18n.* in the LanguageSwitcher's OWN QML context — the
                // exact Item scope where `locale` was shadowed by QLocale. This is
                // the real previously-broken path: if the rename didn't fix it,
                // these evaluate to ERR. Drives EN↔FR↔AR like a menu click and
                // checks direction + live retranslation + persistence + labels.
                QObject* sw = rootObj2->findChild<QObject*>("languageSwitcher");
                if (sw) {
                    QQmlContext* sctx = qmlContext(sw);
                    auto evalS = [&](const char* e) -> QString {
                        QQmlExpression x(sctx, sw, QString::fromLatin1(e));
                        const QVariant v = x.evaluate();
                        return x.hasError() ? QStringLiteral("ERR") : v.toString();
                    };
                    auto run = [&](const char* e) {
                        QQmlExpression x(sctx, sw, QString::fromLatin1(e));
                        x.evaluate();
                    };

                    const QString langCount = evalS("i18n.languages.length");   // 3
                    const QString arLabel   = evalS("i18n.languages[2].label"); // العربية
                    QObject* miAr = rootObj2->findChild<QObject*>("lang_ar");
                    const QString miArText = miAr ? miAr->property("text").toString()
                                                  : QStringLiteral("MISSING");

                    run("i18n.setLanguage('ar')");
                    const int dirAr = static_cast<int>(QGuiApplication::layoutDirection());
                    const QString trAr = QCoreApplication::translate("StatusBadge", "Paid");
                    const bool checkedAr = miAr && miAr->property("checked").toBool();
                    // NEW contexts that were previously falling back to English:
                    const QString arCustomers = QCoreApplication::translate("CustomersScreen", "Customers");
                    const QString arNav       = QCoreApplication::translate("NavRail", "Customers");
                    const QString arName      = QCoreApplication::translate("CustomerEditor", "Name");
                    const QString arReq       = QCoreApplication::translate("CustomerEditor", "This field is required.");
                    const QString arLines     = QCoreApplication::translate("InvoiceEditor", "Line items");
                    // Evaluate the editor's ACTUAL validation function in its own
                    // context (proves the key→qsTr binding produces translated text
                    // live, not just that the catalog has the entry).
                    QString arFieldErr = QStringLiteral("n/a");
                    if (QObject* ce = rootObj2->findChild<QObject*>("customerEditorRoot")) {
                        QQmlExpression fx(qmlContext(ce), ce, QStringLiteral("fieldError(true, 'required')"));
                        const QVariant fv = fx.evaluate();
                        arFieldErr = fx.hasError() ? QStringLiteral("ERR") : fv.toString();
                    }

                    run("i18n.setLanguage('fr')");
                    const int dirFr = static_cast<int>(QGuiApplication::layoutDirection());
                    const QString trFr = QCoreApplication::translate("InvoicesScreen", "New Invoice");
                    const QString frCustomers = QCoreApplication::translate("CustomersScreen", "Customers");
                    const QString frName      = QCoreApplication::translate("CustomerEditor", "Name");

                    run("i18n.setLanguage('en')");
                    const int dirEn = static_cast<int>(QGuiApplication::layoutDirection());
                    const QString curEn = evalS("i18n.language");
                    const QString persisted = QSettings().value(QStringLiteral("ui/language")).toString();

                    pf.write(QString("langCount=%1 arLabel=%2 miArText=%3 checkedAr=%4\n")
                                 .arg(langCount).arg(arLabel).arg(miArText)
                                 .arg(checkedAr ? "true" : "false").toUtf8());
                    pf.write(QString("dirAr=%1 trAr=%2 dirFr=%3 trFr=%4 dirEn=%5 cur=%6 persisted=%7\n")
                                 .arg(dirAr).arg(trAr).arg(dirFr).arg(trFr)
                                 .arg(dirEn).arg(curEn).arg(persisted).toUtf8());
                    pf.write(QString("AR[Customers=%1 Nav=%2 Name=%3 Required=%4 Lines=%5] FR[Customers=%6 Name=%7]\n")
                                 .arg(arCustomers, arNav, arName, arReq, arLines, frCustomers, frName).toUtf8());
                    pf.write(QString("editorValidationLive(AR)=%1\n").arg(arFieldErr).toUtf8());
                }
            }
            // ─────────────────────────────────────────────────────────────────

            // ── Editor VM probe (Dispatch A) ──────────────────────────────────
            // Guarded by ACCT_PROBE_WRITE: these commit() calls mutate real
            // storage (add/edit invoices), so only run when explicitly verifying
            // the write path — not on every probe.
            if (qEnvironmentVariableIsSet("ACCT_PROBE_WRITE")) {

            // ── QML-path interaction regression (catches the setCustomerId bug) ──
            // Drives the EXACT QML idioms via QQmlExpression in the editor's own
            // context: property assignment + lines.setCell(). The old method-call
            // idiom (setCustomerId) is asserted to ERROR. This is the coverage that
            // was missing — C++ probes drove the VM directly and never hit QML.
            if (QObject* ied = engine.rootObjects().first()->findChild<QObject*>("invoiceEditorRoot")) {
                QQmlContext* ictx = qmlContext(ied);
                auto qeval = [&](const char* e, bool* errOut = nullptr) -> QVariant {
                    QQmlExpression x(ictx, ied, QString::fromLatin1(e));
                    const QVariant v = x.evaluate();
                    if (errOut) *errOut = x.hasError();
                    return v;
                };
                invoiceEditor.beginNew();
                const QString errBefore = invoiceEditor.customerError();        // "required"
                int cid = 0;
                const QVariantList opts = invoiceEditor.customerOptions();
                if (!opts.isEmpty()) cid = opts.first().toMap().value("value").toInt();
                // OLD broken idiom must error (regression sentinel):
                bool oldErr = false; qeval("invoiceEditor.setCustomerId(0)", &oldErr);
                // NEW idiom: property assignment + invokable line setter.
                qeval(QString("invoiceEditor.customerId = %1").arg(cid).toUtf8().constData());
                qeval("invoiceEditor.lines.setCell(0, 'description', 'Service')");
                qeval("invoiceEditor.lines.setCell(0, 'qtyText', '2')");
                qeval("invoiceEditor.lines.setCell(0, 'unitPriceText', '50')");
                const int  cidAfter = invoiceEditor.customerId();
                const QString errAfter = invoiceEditor.customerError();         // ""
                const bool validAfter = invoiceEditor.valid();
                const bool committed  = invoiceEditor.commit();
                pf.write(QString("QMLPATH oldIdiomErrors=%1 cidAfter=%2 errBefore=[%3] errAfter=[%4] valid=%5 saved=%6\n")
                             .arg(oldErr ? "true" : "false").arg(cidAfter)
                             .arg(errBefore, errAfter)
                             .arg(validAfter ? "true" : "false")
                             .arg(committed ? "true" : "false").toUtf8());
            }

            invoiceEditor.beginNew();
            const bool v0 = invoiceEditor.valid();   // expect false: no customer, no valid line

            // Fill row 0: qty=2, unitPrice=50, tax=10
            {
                InvoiceDraftLinesModel* lm = invoiceEditor.lines();
                const QModelIndex idx = lm->index(0);
                lm->setData(idx, QStringLiteral("Consulting"),  InvoiceDraftLinesModel::DescriptionRole);
                lm->setData(idx, QStringLiteral("2"),           InvoiceDraftLinesModel::QtyTextRole);
                lm->setData(idx, QStringLiteral("50"),          InvoiceDraftLinesModel::UnitPriceTextRole);
                lm->setData(idx, QStringLiteral("10"),          InvoiceDraftLinesModel::TaxTextRole);
            }

            const QString sub  = invoiceEditor.subtotalText();   // expect "$100.00"
            const QString tot  = invoiceEditor.totalText();       // expect "$110.00"

            // Set a valid customer — pick first option, or use id=0 if none exist
            int firstCustomerId = 0;
            const QVariantList opts = invoiceEditor.customerOptions();
            if (!opts.isEmpty())
                firstCustomerId = opts.first().toMap().value(QStringLiteral("value")).toInt();
            invoiceEditor.setCustomerId(firstCustomerId);

            const bool v1  = invoiceEditor.valid();    // expect true (customer+line set)

            // Snapshot counts BEFORE commit for delta verification
            const int invoicesBefore = static_cast<int>(
                StorageService::instance().invoices().loadAll().size());
            const int linesBefore = static_cast<int>(
                StorageService::instance().invoiceLines().loadAll().size());

            const bool c1  = invoiceEditor.commit();   // expect true

            // Reload to verify storage round-trip
            const int invoicesAfter = static_cast<int>(
                StorageService::instance().invoices().loadAll().size());

            // Delta: how many new lines were added (proves commit wrote the lines)
            const int linesAfter = static_cast<int>(
                StorageService::instance().invoiceLines().loadAll().size()) - linesBefore;

            // invoicesAfter = total after; linesAfter = delta (new lines this commit)
            pf.write(QString("editorValidEmpty=%1 subtotal=%2 total=%3 validReady=%4 committed=%5 invoicesAfter=%6 linesAfter=%7\n")
                         .arg(v0 ? "true" : "false")
                         .arg(sub)
                         .arg(tot)
                         .arg(v1 ? "true" : "false")
                         .arg(c1 ? "true" : "false")
                         .arg(invoicesAfter)   // total non-deleted invoice count (>= 1)
                         .arg(linesAfter)      // delta: lines added by this commit
                         .toUtf8());
            // ─────────────────────────────────────────────────────────────────

            // ── Dispatch B probe: edit round-trip + editor instantiation ──────
            // Pick the last non-deleted invoice to edit (there should be at
            // least the one we just committed above).
            int editTargetId = -1;
            {
                const auto allInvoices = StorageService::instance().invoices().loadAll();
                for (int i = static_cast<int>(allInvoices.size()) - 1; i >= 0; --i) {
                    if (!allInvoices[static_cast<std::size_t>(i)].getIsDeleted()) {
                        editTargetId = static_cast<int>(allInvoices[static_cast<std::size_t>(i)].getId());
                        break;
                    }
                }
            }

            bool editCommitted = false;
            QString dueAfter;
            int editLines = 0;

            if (editTargetId >= 0) {
                invoiceEditor.beginEdit(editTargetId);
                // Mutate due date
                invoiceEditor.setDueDate(QStringLiteral("2030-01-01"));
                editCommitted = invoiceEditor.commit();

                // Reload from storage to verify the write
                if (editCommitted) {
                    try {
                        Invoice reloaded = StorageService::instance().invoices().load(
                            static_cast<uint32_t>(editTargetId));
                        dueAfter = QString::fromStdString(reloaded.getDueDate().toString());
                    } catch (...) {
                        dueAfter = QStringLiteral("load-error");
                    }
                    editLines = static_cast<int>(
                        StorageService::instance().invoiceLines()
                            .findByInvoice(static_cast<uint32_t>(editTargetId)).size());
                }
            }

            // Editor OPEN check: actually open the Popup so its content bindings
            // evaluate (instantiation alone doesn't render Popup contents). Read
            // back rendered height + visible to prove the form laid out.
            bool editorInstantiated = false;
            bool editorOpened = false;
            double editorH = -1.0;
            if (!engine.rootObjects().isEmpty()) {
                QObject* rootObj = engine.rootObjects().first();
                QObject* ed = rootObj->findChild<QObject*>("invoiceEditorRoot");
                editorInstantiated = (ed != nullptr);
                if (ed) {
                    invoiceEditor.beginNew();
                    QMetaObject::invokeMethod(ed, "open");
                    editorOpened = ed->property("visible").toBool();
                    editorH = ed->property("height").toDouble();
                    QMetaObject::invokeMethod(ed, "close");
                }
            }

            pf.write(QString("editCommitted=%1 dueAfter=%2 editLines=%3 editorInstantiated=%4 editorOpened=%5 editorH=%6\n")
                         .arg(editCommitted ? "true" : "false")
                         .arg(dueAfter)
                         .arg(editLines)
                         .arg(editorInstantiated ? "true" : "false")
                         .arg(editorOpened ? "true" : "false")
                         .arg(editorH)
                         .toUtf8());

            // ── Customer probe (Dispatch A) ───────────────────────────────────
            const int custTotal = customersVm.totalCustomers();
            const QString custOut = customersVm.outstandingText();
            customerEditor.beginNew();
            const bool cvEmpty = !customerEditor.valid();            // true: name empty
            customerEditor.setName(QStringLiteral("Acme Co"));
            customerEditor.setEmail(QStringLiteral("x@"));           // bad
            const bool emailRejected = !customerEditor.emailError().isEmpty();
            customerEditor.setEmail(QStringLiteral("a@b.com"));      // good
            const bool cvReady = customerEditor.valid();
            const int custBefore = (int)StorageService::instance().customers().loadAll().size();
            const bool cNew = customerEditor.commit();
            const int custAfter = (int)StorageService::instance().customers().loadAll().size();
            // edit round-trip: edit the just-saved (last) customer's phone
            int editCid = -1; { auto all=StorageService::instance().customers().loadAll(); if(!all.empty()) editCid=(int)all.back().getId(); }
            bool cEdit=false; if(editCid>=0){ customerEditor.beginEdit(editCid); customerEditor.setPhone(QStringLiteral("555-1234")); cEdit=customerEditor.commit(); }
            pf.write(QString("custTotal=%1 custOutstanding=%2 custValidEmpty=%3 custEmailRejected=%4 custReady=%5 custNewCommitted=%6 custAfter=%7 custEditCommitted=%8\n").arg(custTotal).arg(custOut).arg(cvEmpty?"true":"false").arg(emailRejected?"true":"false").arg(cvReady?"true":"false").arg(cNew?"true":"false").arg(custAfter).arg(cEdit?"true":"false").toUtf8());
            // ─────────────────────────────────────────────────────────────────
            } // ACCT_PROBE_WRITE
            // ─────────────────────────────────────────────────────────────────

            pf.close();
        }
        QTimer::singleShot(2000, &qgapp, &QGuiApplication::quit);
    }

    // ── Bidi test-data seed (ACCT_SEED=1) ─────────────────────────────────────
    // One-shot: adds customers with Arabic names + Latin contact info to exercise
    // mixed-direction rendering in lists/forms. Mutates storage; run once.
    if (qEnvironmentVariableIsSet("ACCT_SEED")) {
        struct Seed { const char* name; const char* email; const char* phone; };
        // Names <=31 bytes, emails <=47, phones <=15 (byte limits, Arabic ~2B/char).
        const Seed seeds[] = {
            { "شركة الأمل",       "amal@example.com",            "0551111111" },
            { "محمد عبدالله",     "m.abdullah@example.sa",       "0551234567" },
            { "مؤسسة النور",      "noor@example.com",            "0552222222" },
            { "Globex Trading Co.","billing@globex-intl.example", "0553333333" },
        };
        for (const Seed& s : seeds) {
            customerEditor.beginNew();
            customerEditor.setName(QString::fromUtf8(s.name));
            customerEditor.setEmail(QString::fromUtf8(s.email));
            customerEditor.setPhone(QString::fromUtf8(s.phone));
            customerEditor.commit();
        }
        QTimer::singleShot(200, &qgapp, &QGuiApplication::quit);
        return qgapp.exec();
    }

    // ── Screenshot harness (ACCT_SHOT=<dir>) ──────────────────────────────────
    // Renders the real UI in Arabic, walks every reachable screen + editor (incl.
    // a validation-error state), and grabs the window to PNGs for visual RTL QA.
    // Drive DPI via QT_SCALE_FACTOR=1.25/1.5; window size via ACCT_SHOT_W/H.
    if (qEnvironmentVariableIsSet("ACCT_SHOT") && !engine.rootObjects().isEmpty()) {
        const QString dir = qEnvironmentVariable("ACCT_SHOT");
        QObject*      rootO = engine.rootObjects().first();
        QQuickWindow* win   = qobject_cast<QQuickWindow*>(rootO);
        if (win && qEnvironmentVariableIsSet("ACCT_SHOT_W"))
            win->resize(qEnvironmentVariable("ACCT_SHOT_W").toInt(),
                        qEnvironmentVariable("ACCT_SHOT_H").toInt());

        // Diagnostic log so a failed/empty grab is visible (a null grab saves nothing silently).
        QFile* glog = new QFile(dir + "/grab.log");
        glog->open(QIODevice::WriteOnly | QIODevice::Text);
        glog->write(QString("win=%1 exposed=%2\n")
                        .arg(win ? "ok" : "NULL")
                        .arg(win && win->isVisible() ? "yes" : "no").toUtf8());
        glog->flush();
        auto grab = [win, dir, glog](const QString& name) {
            if (!win) { glog->write("no-win\n"); glog->flush(); return; }
            const QImage img = win->grabWindow();
            const QString path = dir + "/" + name + ".png";
            const bool ok = !img.isNull() && img.save(path);
            glog->write(QString("%1: %2x%3 saved=%4\n")
                            .arg(name).arg(img.width()).arg(img.height())
                            .arg(ok ? "yes" : "NO").toUtf8());
            glog->flush();
        };
        auto openPopup = [rootO](const char* objName) {
            if (QObject* p = rootO->findChild<QObject*>(objName))
                QMetaObject::invokeMethod(p, "open");
        };
        auto closePopup = [rootO](const char* objName) {
            if (QObject* p = rootO->findChild<QObject*>(objName))
                QMetaObject::invokeMethod(p, "close");
        };
        // Inject text into a field (by objectName) for the mixed-bidi rendering test.
        auto setField = [rootO](const char* objName, const QString& v) {
            if (QObject* f = rootO->findChild<QObject*>(objName)) f->setProperty("text", v);
        };

        // Main-scope objects (alive through exec); pass as pointers by value.
        InvoiceEditorViewModel*     ie = &invoiceEditor;
        CustomerEditorViewModel*    ce = &customerEditor;
        PaymentEditorViewModel*     pe = &paymentEditor;
        PaymentAllocationViewModel* pa = &paymentAllocation;
        LedgerExplorerViewModel*    lv = &ledgerVm;
        ExpenseEditorViewModel*     xe = &expenseEditor;
        DiagnosticsViewModel*       dv = &diagnosticsVm;
        LocaleController*           lo = &locale;
        auto setScreen = [rootO](const char* s){ rootO->setProperty("currentScreen", s); };
        auto setLedgerTab = [rootO](const char* t){
            if (QObject* w = rootO->findChild<QObject*>("ledgerWorkspace")) w->setProperty("activeTab", t); };
        auto setSettingsTab = [rootO](const char* t){
            if (QObject* w = rootO->findChild<QObject*>("settingsWorkspace")) w->setProperty("activeTab", t); };

        // Capture language is parameterized (ACCT_SHOT_LANG=en|fr|ar, default ar);
        // the per-language output dir (ACCT_SHOT) keeps baselines separate, so the
        // screenshot names are language-neutral. Capture BY VALUE — this block exits
        // before the timers fire during exec(), so a [&] capture would dangle.
        const QByteArray langBA = qEnvironmentVariable("ACCT_SHOT_LANG", "ar").toUtf8();
        const QString    lang   = QString::fromUtf8(langBA);
        QTimer::singleShot(500,  &qgapp, [lo, lang]{ lo->setLanguage(lang); });
        // Author a few expenses up front so BOTH the ledger and the Expenses screen show real
        // activity (the interaction-test's single expense is voided → nets to nothing). Guarded
        // so repeated shot runs against one data dir (shots.sh runs the exe 6×) don't accumulate.
        QTimer::singleShot(650,  &qgapp, [xe]{
            if (StorageService::instance().expenses().count() > 1) return;   // already populated
            auto add = [xe](const char* dt, const char* amt, int cat, int method, int taxCode){
                xe->beginNew(); xe->setDate(QString::fromLatin1(dt)); xe->setAmount(QString::fromLatin1(amt));
                xe->setCategory(cat); xe->setPaymentMethod(method); xe->setTaxCode(taxCode); xe->commit(); };
            add("2026-08-03", "120.00", 0, 0, 0);    // Office · cash · Standard tax (code 0)
            add("2026-08-01", "800.00", 1, 1, -1);   // Rent · credit (payable) · no tax
            add("2026-08-05", "45.50",  3, 0, 0);    // Travel · cash · Standard tax
            add("2026-08-06", "210.00", 2, 1, -1);   // Utilities · credit (payable)
        });
        QTimer::singleShot(900,  &qgapp, [setScreen]{ setScreen("invoices"); });
        QTimer::singleShot(1300, &qgapp, [grab]{ grab("01_invoices"); });
        QTimer::singleShot(1700, &qgapp, [setScreen]{ setScreen("customers"); });
        QTimer::singleShot(2100, &qgapp, [grab]{ grab("02_customers"); });
        QTimer::singleShot(2500, &qgapp, [setScreen, ie, openPopup]{ setScreen("invoices"); ie->beginNew(); openPopup("invoiceEditorRoot"); });
        QTimer::singleShot(3000, &qgapp, [grab]{ grab("03_invoice_editor"); });
        QTimer::singleShot(3300, &qgapp, [ie]{ ie->commit(); });  // force validation-error state
        QTimer::singleShot(3700, &qgapp, [grab, closePopup]{ grab("04_invoice_editor_errors"); closePopup("invoiceEditorRoot"); });
        QTimer::singleShot(4100, &qgapp, [setScreen, ce, openPopup]{ setScreen("customers"); ce->beginNew(); openPopup("customerEditorRoot"); });
        QTimer::singleShot(4500, &qgapp, [grab]{ grab("05_customer_editor"); });
        // Mixed-bidi rendering test: Arabic + Latin + digits + punctuation in one field.
        QTimer::singleShot(4800, &qgapp, [setField]{
            setField("ce_name",  QString::fromUtf8("شركة ABC-7 #1"));
            setField("ce_email", QStringLiteral("billing.dept@globex-international.test")); });
        QTimer::singleShot(5200, &qgapp, [grab, closePopup]{ grab("06_customer_editor_mixed"); closePopup("customerEditorRoot"); });
        // Payments vertical (populated when ACCT_DATA_DIR carries settlement history):
        // the list + summary, the record editor, and the allocation editor for the
        // first payment (allocatable invoices + existing allocations, all RTL-verified).
        QTimer::singleShot(5600, &qgapp, [setScreen]{ setScreen("payments"); });
        QTimer::singleShot(6000, &qgapp, [grab]{ grab("07_payments"); });
        QTimer::singleShot(6400, &qgapp, [setScreen, pe, openPopup]{ setScreen("payments"); pe->beginNew(); openPopup("paymentEditorRoot"); });
        QTimer::singleShot(6800, &qgapp, [grab, closePopup]{ grab("08_payment_editor"); closePopup("paymentEditorRoot"); });
        QTimer::singleShot(7200, &qgapp, [pa, openPopup]{
            const auto ps = StorageService::instance().audit().listPayments();
            if (!ps.empty()) pa->beginFor(ps.front().id);
            openPopup("allocationEditorRoot"); });
        QTimer::singleShot(7600, &qgapp, [grab, closePopup]{ grab("09_allocation_editor"); closePopup("allocationEditorRoot"); });
        // Ledger workspace (populated when ACCT_DATA_DIR carries posted invoices): the
        // chart of accounts, the journal, the trial balance, and a journal-entry inspector.
        QTimer::singleShot(8000, &qgapp, [setScreen, setLedgerTab]{ setScreen("ledger"); setLedgerTab("accounts"); });
        QTimer::singleShot(8400, &qgapp, [grab]{ grab("10_accounts"); });
        QTimer::singleShot(8700, &qgapp, [setLedgerTab]{ setLedgerTab("journal"); });
        QTimer::singleShot(9100, &qgapp, [grab]{ grab("11_ledger"); });
        QTimer::singleShot(9400, &qgapp, [setLedgerTab]{ setLedgerTab("trial"); });
        QTimer::singleShot(9800, &qgapp, [grab]{ grab("12_trial_balance"); });
        QTimer::singleShot(10000, &qgapp, [setLedgerTab]{ setLedgerTab("tax"); });
        QTimer::singleShot(10100, &qgapp, [grab, setLedgerTab]{ grab("16_tax"); setLedgerTab("trust"); });
        QTimer::singleShot(10150, &qgapp, [grab, setLedgerTab]{ grab("22_trust"); setLedgerTab("journal"); });
        QTimer::singleShot(10200, &qgapp, [lv, openPopup]{
            const auto es = StorageService::instance().audit().listJournalEntries();
            if (!es.empty()) lv->inspect(static_cast<int>(es.front().id));
            openPopup("journalEntryInspectorRoot"); });
        QTimer::singleShot(10600, &qgapp, [grab, closePopup]{ grab("13_journal_entry"); closePopup("journalEntryInspectorRoot"); });
        // Expenses vertical (populated when ACCT_DATA_DIR carries expense events): the list +
        // summary, and the record editor (supplier / date / amount / category / method / memo).
        QTimer::singleShot(11000, &qgapp, [setScreen]{ setScreen("expenses"); });
        QTimer::singleShot(11400, &qgapp, [grab]{ grab("14_expenses"); });
        QTimer::singleShot(11800, &qgapp, [setScreen, xe, openPopup]{ setScreen("expenses"); xe->beginNew(); openPopup("expenseEditorRoot"); });
        QTimer::singleShot(12200, &qgapp, [grab, closePopup]{ grab("15_expense_editor"); closePopup("expenseEditorRoot"); });
        // Settings & System workspace: General preferences, Company details, Backup & Restore,
        // read-only Diagnostics (with verification run), and the About / "coming soon" areas.
        QTimer::singleShot(12600, &qgapp, [setScreen, setSettingsTab]{ setScreen("settings"); setSettingsTab("general"); });
        QTimer::singleShot(13000, &qgapp, [grab]{ grab("17_settings_general"); });
        QTimer::singleShot(13200, &qgapp, [setSettingsTab]{ setSettingsTab("company"); });
        QTimer::singleShot(13500, &qgapp, [grab]{ grab("18_settings_company"); });
        QTimer::singleShot(13700, &qgapp, [setSettingsTab]{ setSettingsTab("backup"); });
        QTimer::singleShot(14000, &qgapp, [grab]{ grab("19_settings_backup"); });
        QTimer::singleShot(14200, &qgapp, [setSettingsTab, dv]{
            setSettingsTab("diagnostics");
            dv->refresh();
            dv->runVerification(); });
        QTimer::singleShot(14700, &qgapp, [grab]{ grab("20_settings_diagnostics"); });
        QTimer::singleShot(14900, &qgapp, [setSettingsTab]{ setSettingsTab("about"); });
        QTimer::singleShot(15200, &qgapp, [grab]{ grab("21_settings_about"); });
        // First-run onboarding wizard (forced visible for the shot; it normally shows only on an
        // empty install). Captures the localized/RTL first-run experience per language.
        QTimer::singleShot(15400, &qgapp, [rootO]{
            if (QObject* w = rootO->findChild<QObject*>("onboardingWizard"))
                QMetaObject::invokeMethod(w, "start"); });
        QTimer::singleShot(15800, &qgapp, [grab]{ grab("23_onboarding"); });
        QTimer::singleShot(16000, &qgapp, &QGuiApplication::quit);
    }

    // ── Startup diagnostics (one line, real launches only) ────────────────────
    // Makes a deployed install self-describing for support: Qt build, writable
    // data path, active locale/direction, load timings. Enable with
    //   QT_LOGGING_RULES="acct.startup=true"
    // It is info-level (on by default) but a single line — not steady-state spam.
    if (!qEnvironmentVariableIsSet("ACCT_SHOT")) {
        const bool writable = QFileInfo(dataPath).isWritable();
        qCInfo(lcStartup).nospace().noquote()
            << "Occountant " << QCoreApplication::applicationVersion()
            << " | Qt " << QLibraryInfo::version().toString()
            << " | data=" << QDir::toNativeSeparators(dataPath)
            << " (writable=" << (writable ? "yes" : "NO")
            << ") | lang=" << locale.language()
            << " dir=" << (QGuiApplication::layoutDirection() == Qt::RightToLeft ? "RTL" : "LTR")
            << " | invoices=" << invoicesVm.totalCount()
            << " | startup: init " << qRound(startupInitMs)
            << "ms + engine " << qRound(startupEngineMs) << "ms"
            << " | diag: " << diag::summary();
        if (!writable)
            qCWarning(lcStorage).noquote()
                << "data directory is not writable — saves will fail:"
                << QDir::toNativeSeparators(dataPath);
    }

    // ── Commercial layer goes live for the real UI run ──
    // Route Qt messages into the rolling production log (redacted), begin scheduled backups, and
    // record the one-screen startup health report for support.
    prodlog::installQtHandler();
    backupScheduler.start();
    platformController.refreshHealth();
    prodlog::info("startup", "health:\n" +
        startupdiag::format(startupdiag::collect(StorageService::instance(), &licenseMgr,
                                                 &backupScheduler, &updateMgr)));

    // Everything above is the startup phase; from here, warnings are runtime.
    diag::enterRuntimePhase();

    // Final warning-counter report at shutdown — makes warning DRIFT over a session
    // visible from the logs alone (compare against the startup-line snapshot).
    QObject::connect(&qgapp, &QGuiApplication::aboutToQuit, &qgapp, [] {
        qCInfo(lcStartup).noquote() << "shutdown | diag:" << diag::summary();
    });

    // engine (stack) is destroyed before `locale`/models/VMs (declared earlier) →
    // clean shutdown, no dangling-context binding warnings.
    return qgapp.exec();
}
