#ifndef QUICK_EXPENSE_LIST_MODEL_H
#define QUICK_EXPENSE_LIST_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <vector>
#include <cstdint>

// Reads the authoritative Expense projection (StorageService::expenses(), event-authored) +
// the supplier name join; void/reversed lineage comes from AuditJournal. Never a write path.
class ExpenseListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        uint32_t id            = 0;
        uint32_t supplierId    = 0;
        QString  supplierName;
        int64_t  amountCents   = 0;   // signed (negative = a reversal expense)
        QString  date;
        uint8_t  category      = 0;
        uint8_t  paymentMethod = 0;
        uint8_t  status        = 0;   // 0=Active, 1=Void
        QString  memo;
        bool     reversed      = false;
    };

    enum Roles {
        ExpenseIdRole = Qt::UserRole + 1,
        SupplierRole,
        AmountTextRole,       // magnitude, e.g. "$50.00"
        DateRole,
        CategoryRole,         // "Office" | "Rent" | ...
        PaymentMethodRole,    // "Cash" | "Credit"
        StatusRole,           // "Active" | "Void"
        IsVoidRole,
        IsReversalRole,       // this row is a negating (reversal) expense
        IsReversedRole,       // this expense was reversed by another
        MemoRole
    };
    Q_ENUM(Roles)

    explicit ExpenseListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

    const std::vector<Row>& rows() const { return rows_; }

    static QString categoryName(uint8_t c);
    static QString methodName(uint8_t m);

private:
    std::vector<Row> rows_;
};

#endif // QUICK_EXPENSE_LIST_MODEL_H
