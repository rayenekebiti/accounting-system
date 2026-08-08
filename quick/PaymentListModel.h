#ifndef QUICK_PAYMENT_LIST_MODEL_H
#define QUICK_PAYMENT_LIST_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <vector>
#include <cstdint>

// Reads the authoritative settlement engine (AuditJournal::listPayments + unallocatedFor);
// never a repository. Status is DERIVED (Unallocated / Partial / Allocated), never stored.
class PaymentListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        uint32_t id           = 0;
        uint32_t customerId   = 0;
        QString  customerName;
        int64_t  amountCents  = 0;
        int64_t  unallocated  = 0;
        QString  date;
    };

    enum Roles {
        PaymentIdRole  = Qt::UserRole + 1,
        CustomerRole,
        AmountTextRole,
        DateRole,
        StatusRole,          // "Unallocated" | "Partial" | "Allocated"
        UnallocatedTextRole,
        HasUnallocatedRole
    };
    Q_ENUM(Roles)

    explicit PaymentListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    const std::vector<Row>& rows() const { return rows_; }

private:
    static QString statusOf(int64_t amount, int64_t unallocated);
    std::vector<Row> rows_;
};

#endif // QUICK_PAYMENT_LIST_MODEL_H
