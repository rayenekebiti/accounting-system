#ifndef QUICK_SUPPLIERS_VIEW_MODEL_H
#define QUICK_SUPPLIERS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "SupplierListModel.h"
#include "SupplierFilterProxy.h"

// Mirrors CustomersViewModel. "Outstanding" here is total payables (owed TO suppliers).
class SuppliersViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel  READ listModel  CONSTANT)

    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(int     totalSuppliers   READ totalSuppliers   NOTIFY summaryChanged)
    Q_PROPERTY(int     owingCount        READ owingCount       NOTIFY summaryChanged)
    Q_PROPERTY(QString outstandingText  READ outstandingText  NOTIFY summaryChanged)
    Q_PROPERTY(int     withBalanceCount READ withBalanceCount NOTIFY summaryChanged)

public:
    explicit SuppliersViewModel(SupplierListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int     totalCount()       const;
    int     filteredCount()    const;
    int     totalSuppliers()   const;
    int     owingCount()       const;
    QString outstandingText()  const;
    int     withBalanceCount() const;

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    SupplierListModel*   source_;
    SupplierFilterProxy* proxy_;

    int    totalSuppliers_   = 0;
    int    withBalanceCount_ = 0;
    double outstandingSum_   = 0.0;
};

#endif // QUICK_SUPPLIERS_VIEW_MODEL_H
