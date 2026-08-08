#pragma once
#include <QString>
#include <QSettings>

// Reads/writes the same QSettings keys the Settings → Numbering panel manages.
// Call reserveXxx() exactly once per new document — it increments the counter.
class NumberingService {
public:
    static QString reserveInvoiceNumber()
    {
        QSettings s;
        const QString prefix = s.value("numbering/invoicePrefix", "INV-").toString();
        int next = s.value("numbering/nextInvoice", "1001").toString().toInt();
        if (next <= 0) next = 1001;
        s.setValue("numbering/nextInvoice", QString::number(next + 1));
        s.sync();
        return prefix + QString::number(next);
    }

    static QString reservePaymentNumber()
    {
        QSettings s;
        const QString prefix = s.value("numbering/paymentPrefix", "PAY-").toString();
        int next = s.value("numbering/nextPayment", "1001").toString().toInt();
        if (next <= 0) next = 1001;
        s.setValue("numbering/nextPayment", QString::number(next + 1));
        s.sync();
        return prefix + QString::number(next);
    }

    // Peek without incrementing (for display only).
    static QString peekInvoiceNumber()
    {
        QSettings s;
        const QString prefix = s.value("numbering/invoicePrefix", "INV-").toString();
        int next = s.value("numbering/nextInvoice", "1001").toString().toInt();
        if (next <= 0) next = 1001;
        return prefix + QString::number(next);
    }

    static QString peekPaymentNumber()
    {
        QSettings s;
        const QString prefix = s.value("numbering/paymentPrefix", "PAY-").toString();
        int next = s.value("numbering/nextPayment", "1001").toString().toInt();
        if (next <= 0) next = 1001;
        return prefix + QString::number(next);
    }
};
