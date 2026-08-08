import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import App

ApplicationWindow {
    id: window
    visible: true
    width:   1080
    height:  700
    title:   "Occountant"
    color:   Theme.color.canvas

    LayoutMirroring.enabled:         Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    // Keep Theme.rtl in sync with the locale controller (exposed as `i18n`).
    Connections {
        target: i18n
        function onLanguageChanged() { Theme.rtl = i18n.rtl }
    }

    // ── Active screen ─────────────────────────────────────────────────────────
    property string currentScreen: "invoices"

    // ── Editor instances (instantiated once, never destroyed) ─────────────────
    InvoiceEditor {
        id: editor
        // No customers yet? Open the customer editor OVER this sheet (Popups stack). On save,
        // Main's customerEditor.onSaved refreshes the invoice's customer picker (see below).
        onAddCustomerRequested: {
            customerEditor.beginNew()
            custEditor.open()
        }
    }
    CustomerEditor        { id: custEditor  }
    SupplierEditor        { id: supEditor   }
    PaymentEditor         { id: payEditor   }
    AllocationEditor      { id: allocEditor }
    ExpenseEditor         { id: expEditor   }
    TaxCodeEditor         { id: taxEditor   }
    JournalEntryInspector {
        id: entryInspector
        onAccountActivated: (accountId) => {
            ledgerVm.setAccountScope(accountId)
            ledgerWorkspace.activeTab = "journal"
            entryInspector.close()
        }
    }

    // ── Invoice VM signals ─────────────────────────────────────────────────────
    Connections {
        target: invoiceEditor
        function onSaved() {
            editor.close()
            invoicesVm.refresh()
            // A posted invoice records a balanced revenue entry — refresh the ledger views.
            accountsVm.refresh()
            ledgerVm.refresh()
            trialBalanceModel.refresh()
            taxSummaryVm.refresh()   // a posted invoice records output tax
        }
    }

    // ── Customer VM signals ────────────────────────────────────────────────────
    Connections {
        target: customerEditor
        function onSaved() {
            custEditor.close()
            customersVm.refresh()
            invoiceEditor.refreshCustomerOptions()   // a new customer may now exist
        }
    }

    // ── Supplier VM signals ────────────────────────────────────────────────────
    Connections {
        target: supplierEditor
        function onSaved() {
            supEditor.close()
            suppliersVm.refresh()
        }
    }

    // ── Payment VM signals ──────────────────────────────────────────────────────
    Connections {
        target: paymentEditor
        function onSaved() {
            payEditor.close()
            paymentsVm.refresh()
            // Flow straight into allocation of the just-recorded payment.
            paymentAllocation.beginFor(paymentEditor.lastPaymentId)
            allocEditor.open()
        }
    }
    Connections {
        target: paymentAllocation
        function onAllocated() {
            paymentsVm.refresh()
            invoicesVm.refresh()    // derived outstanding / payment status may have changed
            customersVm.refresh()   // paid / credit may have changed
        }
    }

    // ── Expense VM signals ──────────────────────────────────────────────────────
    Connections {
        target: expenseEditor
        function onSaved() {
            expEditor.close()       // harmless if a void (the editor was never opened)
            expensesVm.refresh()
            // An expense posts a balanced entry (incl. recoverable tax) — refresh the ledger + tax.
            accountsVm.refresh()
            ledgerVm.refresh()
            trialBalanceModel.refresh()
            taxSummaryVm.refresh()
        }
    }

    // ── Tax-code VM signals ─────────────────────────────────────────────────────
    Connections {
        target: taxCodeEditor
        function onSaved() {
            taxEditor.close()
            taxSummaryVm.refresh()   // the registry (and any derived views) changed
        }
    }

    property int pendingVoidExpenseId: -1
    ConfirmDialog {
        id:             confirmVoidExpense
        title:          qsTr("Void this expense?")
        message:        qsTr("This marks the expense void and posts a compensating ledger entry (open period only).")
        confirmText:    qsTr("Void")
        cancelText:     qsTr("Cancel")
        confirmVariant: "danger"
        onConfirmed: expenseEditor.voidExpense(window.pendingVoidExpenseId)
        onCancelled: { }
    }

    // ── Layout: NavRail (inline-start) + StackLayout (fills remaining space) ───
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // NavRail mirrors to the right in RTL via LayoutMirroring on the window
        NavRail {
            id: navRail
            current: window.currentScreen
            Layout.fillHeight: true
            onNavigate: (key) => { window.currentScreen = key }
        }

        // Content stack — both screens stay alive (no churn)
        StackLayout {
            Layout.fillWidth:  true
            Layout.fillHeight: true
            currentIndex: window.currentScreen === "customers" ? 1
                        : window.currentScreen === "suppliers" ? 2
                        : window.currentScreen === "payments"  ? 3
                        : window.currentScreen === "expenses"  ? 4
                        : window.currentScreen === "ledger"    ? 5
                        : window.currentScreen === "settings"  ? 6 : 0

            // index 0 — Invoices
            InvoicesScreen {
                onNewInvoiceRequested: {
                    invoiceEditor.beginNew()
                    editor.open()
                }
                onRowActivated: (invoiceId) => {
                    invoiceEditor.beginEdit(invoiceId)
                    editor.open()
                }
                onViewLedgerRequested: {
                    // Trace to the ledger: open the Journal scoped to the Revenue account.
                    window.currentScreen = "ledger"
                    ledgerWorkspace.activeTab = "journal"
                    ledgerVm.setAccountScopeByName("Revenue")
                }
            }

            // index 1 — Customers
            CustomersScreen {
                onNewCustomerRequested: {
                    customerEditor.beginNew()
                    custEditor.open()
                }
                onRowActivated: (id) => {
                    customerEditor.beginEdit(id)
                    custEditor.open()
                }
            }

            // index 2 — Suppliers
            SuppliersScreen {
                onNewSupplierRequested: {
                    supplierEditor.beginNew()
                    supEditor.open()
                }
                onRowActivated: (id) => {
                    supplierEditor.beginEdit(id)
                    supEditor.open()
                }
            }

            // index 3 — Payments
            PaymentsScreen {
                onNewPaymentRequested: {
                    paymentEditor.beginNew()
                    payEditor.open()
                }
                onRowActivated: (id) => {
                    paymentAllocation.beginFor(id)
                    allocEditor.open()
                }
            }

            // index 4 — Expenses
            ExpensesScreen {
                onNewExpenseRequested: {
                    expenseEditor.beginNew()
                    expEditor.open()
                }
                onRowActivated: (id) => {
                    expenseEditor.beginEdit(id)
                    expEditor.open()
                }
                onVoidRequested: (id) => {
                    window.pendingVoidExpenseId = id
                    confirmVoidExpense.open()
                }
            }

            // index 5 — Ledger workspace (Accounts / Journal / Trial Balance / Tax)
            LedgerWorkspace {
                id: ledgerWorkspace
                onEntryActivated: (entryId) => {
                    ledgerVm.inspect(entryId)
                    entryInspector.open()
                }
                onNewTaxCodeRequested: {
                    taxCodeEditor.beginNew()
                    taxEditor.open()
                }
            }

            // index 6 — Settings & System (General / Company / Backup / Support / Diagnostics / About)
            SettingsWorkspace {
                id: settingsWorkspace
                onEarlyAccessRequested: earlyAccessDialog.open()
            }
        }
    }

    // ── Crash-recovery UX ──────────────────────────────────────────────────────
    // On startup, if the engine reconciled a crash-leftover journal or discarded an
    // uncommitted tail, settingsVm captured the outcome. If it re-verified cleanly we show a
    // brief reassurance; if verification FAILED we block with a recovery screen — never
    // continue silently on suspect data.
    RecoveryDialog { id: recoveryDialog }
    RecoveryBlocker { id: recoveryBlocker }

    Component.onCompleted: {
        Theme.rtl = i18n.rtl
        if (settingsVm.recoveryOccurred) {
            if (settingsVm.recoveryVerified) recoveryDialog.open()
            else                             recoveryBlocker.open()
        }
        // Early Access notice: only when onboarding isn't showing and no recovery dialog is up, so a
        // fresh install sees onboarding first (the notice appears on the next, settled launch).
        else if (earlyAccessVm.shouldShow && !onboardingVm.needed)
            earlyAccessDialog.open()
    }

    // Language switcher — floated in the top-right corner (inline-end)
    Item {
        anchors.top:    parent.top
        anchors.right:  parent.right
        anchors.margins: Theme.space.sm
        width:  36
        height: 36

        LanguageSwitcher {}
    }

    // ── Early Access welcome notice ────────────────────────────────────────────
    // Shown automatically on first launch / after a major update (earlyAccessVm.shouldShow), but
    // only once onboarding isn't taking the screen — never two overlays at once. Also reachable
    // manually from About. Writes only preferences; authors no accounting events.
    EarlyAccessDialog { id: earlyAccessDialog; z: 900 }

    // ── First-run onboarding gate ──────────────────────────────────────────────
    // A full-window overlay shown BEFORE the app on a fresh install. It writes only settings
    // (no accounting events). Once completed or explicitly skipped it hides and never returns
    // (onboardingVm.needed becomes false). Existing users never see it.
    OnboardingWizard {
        id: onboarding
        z: 1000
        Component.onCompleted: if (onboardingVm.needed) start()
    }
}
