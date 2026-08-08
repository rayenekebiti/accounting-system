#ifndef QUICK_ITEST_H
#define QUICK_ITEST_H

// Lightweight QML interaction-test layer. Drives the REAL app's QML through the
// same paths a user would (property assignment, invokable model APIs, delegate
// signal handlers) via QQmlExpression / QMetaObject::invokeMethod, asserts VM
// state + persistence, and FAILS on any QML runtime error. No event loop, no
// pixel automation, no async waits. Invoked when ACCT_ITEST is set.
//
// Returns the number of failed assertions (0 = all passed). main returns this
// as the process exit code so CI can gate on it.

class QQmlApplicationEngine;
class InvoiceEditorViewModel;
class CustomerEditorViewModel;
class SupplierEditorViewModel;
class PaymentEditorViewModel;
class PaymentAllocationViewModel;
class LocaleController;

int runInteractionTests(QQmlApplicationEngine& engine,
                        InvoiceEditorViewModel& invoiceEditor,
                        CustomerEditorViewModel& customerEditor,
                        SupplierEditorViewModel& supplierEditor,
                        PaymentEditorViewModel& paymentEditor,
                        PaymentAllocationViewModel& paymentAllocation,
                        LocaleController& locale);

#endif // QUICK_ITEST_H
