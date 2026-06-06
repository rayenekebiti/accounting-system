#ifndef UI_MODELS_INVOICE_TABLE_MODEL_H
#define UI_MODELS_INVOICE_TABLE_MODEL_H
#include "Invoice.h"
#include <QAbstractTableModel>
#include <vector>

class InvoiceTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColNumber = 0, ColCustomerId, ColIssueDate, ColDueDate, ColTotal, ColStatus, ColCount };

    explicit InvoiceTableModel(QObject* parent = nullptr);

    void setRows(std::vector<Invoice> rows);
    void appendRow(const Invoice& inv);
    void updateRow(int row, const Invoice& inv);
    void removeRow(int row);
    const Invoice& at(int row) const;
    int liveCount() const;

    int rowCount(const QModelIndex& = QModelIndex()) const override;
    int columnCount(const QModelIndex& = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role) const override;

private:
    std::vector<Invoice> rows_;
};

#endif
