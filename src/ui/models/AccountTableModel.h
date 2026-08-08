#pragma once
#include "Account.h"
#include <QAbstractTableModel>
#include <memory>
#include <vector>

class AccountTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { ColName = 0, ColType, ColBalance, ColCount };

    explicit AccountTableModel(QObject* parent = nullptr);

    void setRows(std::vector<std::unique_ptr<Account>> rows);
    const Account& at(int row) const;
    int liveCount() const;

    int     rowCount   (const QModelIndex& = {}) const override;
    int     columnCount(const QModelIndex& = {}) const override;
    QVariant data      (const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orient, int role = Qt::DisplayRole) const override;

private:
    std::vector<std::unique_ptr<Account>> rows_;

    static QString typeName(AccountType t);
};
