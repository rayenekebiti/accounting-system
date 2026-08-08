#ifndef QUICK_INVOICE_LIST_MODEL_H
#define QUICK_INVOICE_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <vector>
#include "core/Invoice.h"

class InvoiceListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        NumberRole    = Qt::UserRole + 1,
        CustomerRole,
        IssueDateRole,
        DueDateRole,
        TotalTextRole,
        StatusRole,
        InvoiceIdRole,
        OutstandingTextRole,   // derived from the settlement engine (audit().outstandingFor)
        SettledTextRole,       // derived (audit().settledFor)
        PaymentStatusRole      // "Paid" | "Partial" | "Unpaid" (settlement-derived)
    };
    Q_ENUM(Roles)

    explicit InvoiceListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    // Expose invoices for KPI computation in AppController
    const std::vector<Invoice>& invoices() const { return invoices_; }

private:
    std::vector<Invoice>        invoices_;
    std::vector<int64_t>        outstandingCents_;   // parallel to invoices_ (settlement-derived)
    std::vector<int64_t>        settledCents_;        // parallel to invoices_ (settlement-derived)
    QHash<uint32_t, QString>    customerNames_;

    static QString statusString(InvoiceStatus s);
};

#endif // QUICK_INVOICE_LIST_MODEL_H
