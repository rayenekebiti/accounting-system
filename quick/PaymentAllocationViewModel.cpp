#include "PaymentAllocationViewModel.h"
#include "storage/StorageService.h"
#include "core/Money.h"
#include "core/IsoDate.h"
#include <QVariantMap>
#include <stdexcept>

PaymentAllocationViewModel::PaymentAllocationViewModel(QObject* parent)
    : QObject(parent)
{}

void PaymentAllocationViewModel::beginFor(int paymentId)
{
    paymentId_ = paymentId;
    customerId_ = 0; customerName_.clear(); amountCents_ = 0; unallocated_ = 0; payDate_.clear();
    allocatable_.clear(); existing_.clear();

    if (StorageService::instance().isInitialized()) {
        for (const auto& p : StorageService::instance().audit().listPayments()) {
            if (static_cast<int>(p.id) == paymentId) {
                customerId_  = p.customerId;
                amountCents_ = p.amountCents;
                payDate_     = QString::fromStdString(p.date.toString());
                break;
            }
        }
    }
    reload();
}

void PaymentAllocationViewModel::reload()
{
    allocatable_.clear();
    existing_.clear();
    if (!StorageService::instance().isInitialized() || paymentId_ < 0) { emit changed(); return; }

    auto& storage = StorageService::instance();
    auto& aj = storage.audit();

    customerName_ = QString::fromUtf8(storage.customers().load(customerId_).getName());
    unallocated_  = aj.unallocatedFor(static_cast<uint32_t>(paymentId_));

    // Allocatable = the customer's invoices with derived outstanding > 0.
    for (const auto& inv : storage.invoices().findByCustomer(customerId_)) {
        const int64_t out = aj.outstandingFor(inv.getId());
        if (out <= 0) continue;
        QVariantMap m;
        m.insert(QStringLiteral("invoiceId"),       static_cast<int>(inv.getId()));
        m.insert(QStringLiteral("number"),          QString::fromUtf8(inv.getInvoiceNumber()));
        m.insert(QStringLiteral("outstandingCents"), static_cast<double>(out));
        m.insert(QStringLiteral("outstandingText"),  QString("$%1").arg(out / 100.0, 0, 'f', 2));
        allocatable_.append(m);
    }

    // Existing allocations of THIS payment (including reversed, for the lineage view).
    for (const auto& a : aj.allocationsForPayment(static_cast<uint32_t>(paymentId_))) {
        QVariantMap m;
        m.insert(QStringLiteral("allocationId"),  static_cast<int>(a.id));
        m.insert(QStringLiteral("invoiceNumber"), QString::fromUtf8(storage.invoices().load(a.invoiceId).getInvoiceNumber()));
        m.insert(QStringLiteral("amountText"),    QString("$%1").arg(a.amountCents / 100.0, 0, 'f', 2));
        m.insert(QStringLiteral("reversed"),      a.reversed);
        existing_.append(m);
    }

    emit changed();
}

bool PaymentAllocationViewModel::allocate(int invoiceId, const QString& amountText)
{
    const int64_t cents = Money::fromDouble(amountText.toDouble()).cents();
    if (cents <= 0) { emit actionFailed(QStringLiteral("positiveAmount")); return false; }
    try {
        auto& aj = StorageService::instance().audit();
        const auto date = IsoDate::fromString(payDate_.toStdString());
        // AUTHORITY: allocation is an authoritative event; the engine enforces amount <= unallocated.
        aj.allocatePayment(static_cast<uint32_t>(paymentId_), static_cast<uint32_t>(invoiceId),
                           cents, date.value_or(IsoDate{}), StorageService::now());
    } catch (const std::exception& e) {
        emit actionFailed(QString::fromUtf8(e.what()));
        return false;
    }
    reload();
    emit allocated();
    return true;
}

bool PaymentAllocationViewModel::reverse(int allocationId)
{
    try {
        // AUTHORITY: reversal is an append-only event (never a delete of settlement history).
        StorageService::instance().audit().reverseAllocation(static_cast<uint32_t>(allocationId), StorageService::now());
    } catch (const std::exception& e) {
        emit actionFailed(QString::fromUtf8(e.what()));
        return false;
    }
    reload();
    emit allocated();
    return true;
}

void PaymentAllocationViewModel::refresh() { reload(); }
