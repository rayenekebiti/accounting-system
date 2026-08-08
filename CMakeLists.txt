cmake_minimum_required(VERSION 3.16)
project(AccountingSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent PrintSupport Sql Qml Quick QuickControls2 LinguistTools)
# OpenSSL libcrypto — Ed25519 for asymmetric UPDATE-payload signing (app/Ed25519Signature.cpp).
find_package(OpenSSL REQUIRED)

option(ACCT_USE_SQLITE "Use SQLite backend instead of binary flat-files" OFF)

# Embed the Ed25519 update-signing SECRET (dev keypair) so CI/gates + the offline vendor signing
# tool can produce signatures. Turn OFF for a shipped release artifact: the binary then verifies
# updates but cannot sign them (no secret compiled in). tools/release.sh passes -DACCT_DEV_SIGNING=OFF.
option(ACCT_DEV_SIGNING "Compile in the Ed25519 update-signing secret (OFF for release artifacts)" ON)

# ── Deterministic build provenance (embedded via app/BuildInfo) ───────────────
# Captured at configure time from git. tools/release.sh overrides the channel/build-id below.
# The timestamp is the COMMIT time (git-authored), never a wall clock — so BuildInfo for a given
# commit is reproducible. Nothing here touches the accounting engine.
find_package(Git QUIET)
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ACCT_GIT_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} status --porcelain
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE _acct_git_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} show -s --format=%ct HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE ACCT_BUILD_EPOCH OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
if(NOT ACCT_GIT_COMMIT)
    set(ACCT_GIT_COMMIT "unknown")
endif()
if("${_acct_git_status}" STREQUAL "")
    set(ACCT_GIT_DIRTY 0)
else()
    set(ACCT_GIT_DIRTY 1)
endif()
if(NOT ACCT_BUILD_EPOCH)
    set(ACCT_BUILD_EPOCH 0)
endif()

# Release stamping: tools/release.sh passes these; a bare local build leaves them empty (falling
# back to AppInfo.h's "dev"/"stable" defaults).
set(ACCT_RELEASE_CHANNEL  "" CACHE STRING "Channel to stamp into the build (stable|rc|beta|development)")
set(ACCT_RELEASE_BUILD_ID "" CACHE STRING "Build id to stamp into the build")

# Locate windeployqt early so BOTH executable targets can deploy against it.
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")
endif()

set(CORE_SOURCES
    storage/BinaryRecordFile.cpp
    storage/EventLog.cpp
    storage/AuditJournal.cpp
    storage/CompatibilityManifest.cpp
    core/Category.cpp
    core/InvoiceLine.cpp
    core/budget.cpp
    core/AccountingSystem.cpp
    core/Customer.cpp
    core/Supplier.cpp
    core/Expense.cpp
    core/TaxCode.cpp
    core/Product.cpp
    core/Invoice.cpp
    core/Payment.cpp
    core/Money.cpp
    core/IsoDate.cpp
    core/Account.cpp
    core/BankAccount.cpp
    core/CashAccount.cpp
    core/CheckingAccount.cpp
    core/SavingsAccount.cpp
    transaction/transaction.cpp
    transaction/IncomeTransaction.cpp
    transaction/ExpenseTransaction.cpp
    transaction/RecurringIncome.cpp
    transaction/RecurringExpense.cpp
)

set(MODEL_SOURCES
    src/ui/models/CustomerTableModel.cpp
    src/ui/models/SupplierTableModel.cpp
    src/ui/models/ProductTableModel.cpp
    src/ui/models/InvoiceTableModel.cpp
    src/ui/models/PaymentTableModel.cpp
    src/ui/models/CategoryTableModel.cpp
    src/ui/models/BudgetTableModel.cpp
    src/ui/models/TransactionTableModel.cpp
    src/ui/models/AccountTableModel.cpp
)

set(SERVICE_SOURCES
    src/ui/services/Exporter.cpp
    src/ui/services/InvoicePrinter.cpp
    storage/BackupService.cpp
    src/ui/common/UiErrorBus.cpp
)

set(DIALOG_SOURCES
    src/ui/dialogs/CustomerEditorDialog.cpp
    src/ui/dialogs/InvoiceEditorDialog.cpp
    src/ui/dialogs/SupplierEditorDialog.cpp
    src/ui/dialogs/ProductEditorDialog.cpp
    src/ui/dialogs/PaymentEditorDialog.cpp
)

set(UI_SOURCES
    src/ui/theme/ThemeManager.cpp
    src/ui/app/MainWindow.cpp
    src/ui/app/NavigationPanel.cpp
    src/ui/app/GlobalToolBar.cpp
    src/ui/app/PageHeader.cpp
    src/ui/app/PageRouter.cpp
    src/ui/pages/base/Page.cpp
    src/ui/pages/base/ListPage.cpp
    src/ui/pages/dashboard/DashboardPage.cpp
    src/ui/pages/customers/CustomersPage.cpp
    src/ui/pages/suppliers/SuppliersPage.cpp
    src/ui/pages/products/ProductsPage.cpp
    src/ui/pages/invoices/InvoicesPage.cpp
    src/ui/pages/payments/PaymentsPage.cpp
    src/ui/pages/reports/ReportsPage.cpp
    src/ui/pages/settings/SettingsPage.cpp
    src/ui/components/tables/DataTableView.cpp
    src/ui/components/tables/PaginationFooter.cpp
    src/ui/components/tables/RowActionsDelegate.cpp
    src/ui/components/display/MiniBarChart.cpp
    src/ui/components/inputs/SearchBar.cpp
    src/ui/components/inputs/FilterBar.cpp
    src/ui/components/forms/FormRow.cpp
    src/ui/components/forms/SectionHeader.cpp
    src/ui/components/feedback/BusyOverlay.cpp
    src/ui/components/feedback/EmptyStateWidget.cpp
    src/ui/components/feedback/ConfirmDialog.cpp
    src/ui/components/display/KpiCard.cpp
    src/ui/components/display/StatusBadge.cpp
    src/ui/components/display/MoneyLabel.cpp
)

add_executable(AccountingSystem main.cpp ${CORE_SOURCES} ${MODEL_SOURCES} ${SERVICE_SOURCES} ${DIALOG_SOURCES} ${UI_SOURCES})

target_include_directories(AccountingSystem PRIVATE src/ui core .)

target_link_libraries(AccountingSystem PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::PrintSupport
    Qt6::Sql
)

add_executable(AccountingQuick
    quick/main_quick.cpp
    quick/InvoiceListModel.cpp
    quick/AppController.cpp
    quick/InvoiceFilterProxy.cpp
    quick/InvoicesViewModel.cpp
    quick/InvoiceDraftLinesModel.cpp
    quick/InvoiceEditorViewModel.cpp
    quick/LocaleController.cpp
    quick/CustomerListModel.cpp
    quick/CustomerFilterProxy.cpp
    quick/CustomersViewModel.cpp
    quick/CustomerEditorViewModel.cpp
    quick/SupplierListModel.cpp
    quick/SupplierFilterProxy.cpp
    quick/SuppliersViewModel.cpp
    quick/SupplierEditorViewModel.cpp
    quick/PaymentListModel.cpp
    quick/PaymentFilterProxy.cpp
    quick/PaymentsViewModel.cpp
    quick/PaymentEditorViewModel.cpp
    quick/PaymentAllocationViewModel.cpp
    quick/AccountsListModel.cpp
    quick/AccountsFilterProxy.cpp
    quick/AccountsViewModel.cpp
    quick/LedgerEntriesModel.cpp
    quick/TrialBalanceModel.cpp
    quick/LedgerExplorerViewModel.cpp
    quick/ExpenseListModel.cpp
    quick/ExpenseFilterProxy.cpp
    quick/ExpensesViewModel.cpp
    quick/ExpenseEditorViewModel.cpp
    quick/TaxCodeListModel.cpp
    quick/TaxSummaryViewModel.cpp
    quick/TaxCodeEditorViewModel.cpp
    quick/DiagnosticsViewModel.cpp
    quick/SettingsViewModel.cpp
    quick/PeriodCloseViewModel.cpp
    quick/OnboardingViewModel.cpp
    quick/EarlyAccessViewModel.cpp
    quick/SupportCenterViewModel.cpp
    quick/ExportService.cpp
    quick/ExportViewModel.cpp
    quick/BackupViewModel.cpp
    quick/itest.cpp
    quick/accept.cpp
    quick/hostile_accept.cpp
    quick/pilot_checks.cpp
    quick/early_access.cpp
    quick/bench.cpp
    quick/ptest.cpp
    quick/fuzz.cpp
    quick/perf.cpp
    quick/endure.cpp
    quick/a11y.cpp
    quick/logging.cpp
    quick/diagnostics.cpp
    quick/c2test.cpp
    quick/PlatformController.cpp
    app/Signature.cpp
    app/Ed25519Signature.cpp
    app/Logging.cpp
    app/LicenseManager.cpp
    app/BackupScheduler.cpp
    app/UpdateManager.cpp
    app/MiniZip.cpp
    app/CrashReporter.cpp
    app/StartupDiagnostics.cpp
    app/BuildInfo.cpp
    app/SupportBundle.cpp
    app/SupportId.cpp
    app/SupportTicketStore.cpp
    ${CORE_SOURCES})

# Embed deterministic build provenance (see app/BuildInfo). release.sh overrides channel/build-id.
target_compile_definitions(AccountingQuick PRIVATE
    ACCT_GIT_COMMIT="${ACCT_GIT_COMMIT}"
    ACCT_GIT_DIRTY=${ACCT_GIT_DIRTY}
    ACCT_BUILD_EPOCH=${ACCT_BUILD_EPOCH})
if(ACCT_RELEASE_CHANNEL)
    target_compile_definitions(AccountingQuick PRIVATE ACCT_CHANNEL="${ACCT_RELEASE_CHANNEL}")
endif()
if(ACCT_RELEASE_BUILD_ID)
    target_compile_definitions(AccountingQuick PRIVATE ACCT_BUILD_ID="${ACCT_RELEASE_BUILD_ID}")
endif()

set_source_files_properties(quick/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(AccountingQuick
    URI App
    VERSION 1.0
    QML_FILES
        quick/qml/Theme.qml
        quick/qml/Main.qml
        quick/qml/AppButton.qml
        quick/qml/IconButton.qml
        quick/qml/Card.qml
        quick/qml/Badge.qml
        quick/qml/Chip.qml
        quick/qml/AppTextField.qml
        quick/qml/SearchField.qml
        quick/qml/Avatar.qml
        quick/qml/Divider.qml
        quick/qml/StatCard.qml
        quick/qml/MetricCell.qml
        quick/qml/PageHeader.qml
        quick/qml/FilterBar.qml
        quick/qml/CurrencyAmount.qml
        quick/qml/StatusBadge.qml
        quick/qml/EmptyState.qml
        quick/qml/ListRowCard.qml
        quick/qml/InvoiceRow.qml
        quick/qml/Gallery.qml
        quick/qml/InvoicesScreen.qml
        quick/qml/LanguageSwitcher.qml
        quick/qml/FieldInput.qml
        quick/qml/Select.qml
        quick/qml/ModalSheet.qml
        quick/qml/ConfirmDialog.qml
        quick/qml/InvoiceEditor.qml
        quick/qml/SummaryBar.qml
        quick/qml/CustomersScreen.qml
        quick/qml/CustomerEditor.qml
        quick/qml/SuppliersScreen.qml
        quick/qml/SupplierEditor.qml
        quick/qml/PaymentsScreen.qml
        quick/qml/PaymentEditor.qml
        quick/qml/AllocationEditor.qml
        quick/qml/ExpensesScreen.qml
        quick/qml/ExpenseEditor.qml
        quick/qml/LedgerWorkspace.qml
        quick/qml/AccountsScreen.qml
        quick/qml/LedgerExplorer.qml
        quick/qml/TrialBalanceScreen.qml
        quick/qml/TaxSummaryScreen.qml
        quick/qml/TrustDashboard.qml
        quick/qml/PeriodCloseScreen.qml
        quick/qml/EarlyAccessDialog.qml
        quick/qml/SupportCenter.qml
        quick/qml/OnboardingWizard.qml
        quick/qml/TaxCodeEditor.qml
        quick/qml/JournalEntryInspector.qml
        quick/qml/NavRail.qml
        quick/qml/SettingsWorkspace.qml
        quick/qml/GeneralSettings.qml
        quick/qml/CompanySettings.qml
        quick/qml/BackupScreen.qml
        quick/qml/DiagnosticsScreen.qml
        quick/qml/AboutScreen.qml
        quick/qml/RecoveryDialog.qml
        quick/qml/RecoveryBlocker.qml
)

target_include_directories(AccountingQuick PRIVATE core . quick app)
target_link_libraries(AccountingQuick PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 OpenSSL::Crypto)
if(ACCT_DEV_SIGNING)
    target_compile_definitions(AccountingQuick PRIVATE ACCT_DEV_SIGNING)
endif()
if(WIN32)
    target_link_libraries(AccountingQuick PRIVATE psapi)  # GetProcessMemoryInfo (working-set bench)
endif()

qt_add_translations(AccountingQuick
    TS_FILES
        quick/i18n/app_en.ts
        quick/i18n/app_fr.ts
        quick/i18n/app_ar.ts
    RESOURCE_PREFIX "/i18n"
)

if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET AccountingQuick POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --no-quick-import
            "$<TARGET_FILE:AccountingQuick>"
        COMMENT "Deploying Qt Quick runtime (windeployqt, no QML scan)..."
        VERBATIM
    )

    # windeployqt on MinGW does NOT copy the MSYS2 third-party DLL closure (ICU,
    # harfbuzz, freetype, PCRE2, glib, …) or the QtQuick style-module DLLs skipped
    # by --no-quick-import. Without them the app dies with STATUS_DLL_NOT_FOUND on a
    # machine without MSYS2 on PATH. This target completes the deployment closure so
    # the build is genuinely clean-machine deployable. Run explicitly (it adds ~1.5 min):
    #   cmake --build build --target deploy_quick
    find_program(BASH_EXECUTABLE bash HINTS "C:/msys64/usr/bin" "/usr/bin")
    if(BASH_EXECUTABLE)
        add_custom_target(deploy_quick
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/deploy-deps.sh"
                    "$<TARGET_FILE_DIR:AccountingQuick>"
            DEPENDS AccountingQuick
            COMMENT "Bundling MSYS2 third-party DLL closure for clean-machine deployment..."
            USES_TERMINAL
            VERBATIM
        )
    endif()
endif()

if(ACCT_USE_SQLITE)
    target_compile_definitions(AccountingSystem PRIVATE ACCT_USE_SQLITE)
endif()

# ── Vendor license generator (developer-only) ─────────────────────────────────
# A separate console tool that signs customer licenses with the vendor Ed25519 PRIVATE key. Built
# ONLY when ACCT_DEV_SIGNING is ON (the vendor/dev machine); the release build (ACCT_DEV_SIGNING=OFF)
# has no private key, so this target is not produced and is never part of the installer. See
# docs/license-generation.md. Reuses the shared licensing + signature code (no parallel system).
if(ACCT_DEV_SIGNING)
    add_executable(license_gen
        tools/license_gen.cpp
        app/LicenseManager.cpp
        app/Signature.cpp
        app/Ed25519Signature.cpp
        app/Logging.cpp)
    target_include_directories(license_gen PRIVATE . app)
    target_compile_definitions(license_gen PRIVATE ACCT_DEV_SIGNING)
    target_link_libraries(license_gen PRIVATE Qt6::Core OpenSSL::Crypto)

    # license_gen depends on Qt6Core/libcrypto/libstdc++/libgcc. Bare `build/license_gen.exe` only
    # runs because those DLLs happen to sit in build/ (deployed for AccountingQuick) or because
    # MSYS2/ucrt64 is on PATH; copied elsewhere it dies with STATUS_DLL_NOT_FOUND (0xC0000135). This
    # target produces a SELF-CONTAINED, PATH-free build/license_gen_dist/ by bundling its DLL closure
    # (same objdump-recursion the app uses). Run: cmake --build build --target license_gen_dist
    if(BASH_EXECUTABLE)
        add_custom_target(license_gen_dist
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/license_gen_dist"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:license_gen>" "${CMAKE_BINARY_DIR}/license_gen_dist/"
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/deploy-deps.sh" "${CMAKE_BINARY_DIR}/license_gen_dist"
            DEPENDS license_gen
            COMMENT "Bundling license_gen with its DLL closure (self-contained, PATH-free)..."
            VERBATIM)
    endif()
endif()

option(ACCT_BUILD_TESTS "Build the AccountingPro unit-test suite" ON)

if(ACCT_BUILD_TESTS)
    enable_testing()
    add_executable(AccountingTests
        tests/test_main.cpp
        tests/core/test_money.cpp
        tests/core/test_iso_date.cpp
        core/Money.cpp
        core/IsoDate.cpp
    )
    target_include_directories(AccountingTests PRIVATE .)
    # Tests link no Qt — keeps the suite cheap on CI and forces the pure-C++
    # value types to stay Qt-free.
    add_test(NAME AccountingTests COMMAND AccountingTests)
endif()

if(WIN32)
    set_target_properties(AccountingSystem PROPERTIES
        WIN32_EXECUTABLE TRUE
    )

    # Deploy Qt runtime DLLs next to the executable after each build.
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${Qt6_DIR}/../../../bin"
    )
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET AccountingSystem POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:AccountingSystem>"
            COMMENT "Deploying Qt runtime (windeployqt)..."
            VERBATIM
        )
        # windeployqt doesn't always detect Qt6Concurrent — copy it explicitly.
        add_custom_command(TARGET AccountingSystem POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Concurrent>"
                "$<TARGET_FILE_DIR:AccountingSystem>"
            COMMENT "Copying Qt6Concurrent.dll..."
            VERBATIM
        )

        # Copy the UCRT64 C++ runtime DLLs so the exe doesn't pick up a mismatched
        # MINGW64 version from PATH at runtime.
        foreach(_rt libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            find_file(_rt_path "${_rt}" HINTS "${Qt6_DIR}/../../../bin" NO_CACHE)
            if(_rt_path)
                add_custom_command(TARGET AccountingSystem POST_BUILD
                    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${_rt_path}" "$<TARGET_FILE_DIR:AccountingSystem>"
                    COMMENT "Copying ${_rt}..."
                    VERBATIM
                )
                unset(_rt_path)
            endif()
        endforeach()
    endif()
endif()
