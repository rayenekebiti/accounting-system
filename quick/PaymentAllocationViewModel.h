#ifndef QUICK_PAYMENT_ALLOCATION_VIEW_MODEL_H
#define QUICK_PAYMENT_ALLOCATION_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <cstdint>

// Allocates a payment to invoices and reverses allocations — all through the settlement
// engine (AuditJournal::allocatePayment / reverseAllocation). Reads outstanding/unallocated
// (DERIVED); never mutates a balance. Lists are QVariantList for simple Repeater binding.
class PaymentAllocationViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int     paymentId         READ paymentId         NOTIFY changed)
    Q_PROPERTY(QString customerName      READ customerName      NOTIFY changed)
    Q_PROPERTY(QString paymentAmountText READ paymentAmountText NOTIFY changed)
    Q_PROPERTY(QString unallocatedText   READ unallocatedText   NOTIFY changed)
    Q_PROPERTY(bool    hasUnallocated    READ hasUnallocated    NOTIFY changed)
    // {invoiceId, number, outstandingText, outstandingCents}
    Q_PROPERTY(QVariantList allocatableInvoices READ allocatableInvoices NOTIFY changed)
    // {allocationId, invoiceNumber, amountText, reversed}
    Q_PROPERTY(QVariantList existingAllocations READ existingAllocations NOTIFY changed)

public:
    explicit PaymentAllocationViewModel(QObject* parent = nullptr);

    int          paymentId()         const { return paymentId_; }
    QString      customerName()      const { return customerName_; }
    QString      paymentAmountText() const { return QString("$%1").arg(amountCents_ / 100.0, 0, 'f', 2); }
    QString      unallocatedText()   const { return QString("$%1").arg(unallocated_ / 100.0, 0, 'f', 2); }
    bool         hasUnallocated()    const { return unallocated_ > 0; }
    QVariantList allocatableInvoices() const { return allocatable_; }
    QVariantList existingAllocations() const { return existing_; }

    Q_INVOKABLE void beginFor(int paymentId);
    Q_INVOKABLE bool allocate(int invoiceId, const QString& amountText);
    Q_INVOKABLE bool reverse(int allocationId);
    Q_INVOKABLE void refresh();

signals:
    void changed();
    void allocated();                         // a settlement event happened → refresh other views
    void actionFailed(const QString& message);

private:
    void reload();

    int      paymentId_   = -1;
    uint32_t customerId_  = 0;
    QString  customerName_;
    int64_t  amountCents_ = 0;
    int64_t  unallocated_ = 0;
    QString  payDate_;     // the payment's date; allocations are dated to match it

    QVariantList allocatable_;
    QVariantList existing_;
};

#endif // QUICK_PAYMENT_ALLOCATION_VIEW_MODEL_H
