#ifndef UI_MODELS_PAYMENT_TABLE_MODEL_H
#define UI_MODELS_PAYMENT_TABLE_MODEL_H
#include "Payment.h"
#include <QAbstractTableModel>
#include <vector>

class PaymentTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColNumber = 0, ColDate, ColParty, ColInvoiceId, ColAmount, ColMethod, ColCount };

    explicit PaymentTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Payment> rows);
    void appendRow(const Payment& p);
    void updateRow(int row, const Payment& p);
    void removeRow(int row);
    const Payment& at(int row) const;
    int liveCount() const;

    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    std::vector<Payment> rows_;
};

#endif
