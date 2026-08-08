#pragma once
#include "transaction/transaction.h"
#include <QAbstractTableModel>
#include <memory>
#include <vector>

class TransactionTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColId = 0, ColDesc, ColAmount, ColDate, ColCategory, ColType, ColCount };

    explicit TransactionTableModel(QObject* parent = nullptr);

    void setRows(std::vector<std::unique_ptr<Transaction>> rows);
    const Transaction& at(int row) const;
    int liveCount() const;

    int     rowCount   (const QModelIndex& = {}) const override;
    int     columnCount(const QModelIndex& = {}) const override;
    QVariant data      (const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

private:
    std::vector<std::unique_ptr<Transaction>> rows_;

    static QString typeName(TransactionType t);
};
