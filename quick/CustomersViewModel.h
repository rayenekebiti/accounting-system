#ifndef QUICK_CUSTOMERS_VIEW_MODEL_H
#define QUICK_CUSTOMERS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "CustomerListModel.h"
#include "CustomerFilterProxy.h"

class CustomersViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel  READ listModel  CONSTANT)

    // Explicit counts — QAbstractItemModel has no QML `count`, so the UI must
    // NOT rely on listModel.count; these are the robust source of truth.
    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(int     totalCustomers  READ totalCustomers  NOTIFY summaryChanged)
    Q_PROPERTY(int     owingCount      READ owingCount      NOTIFY summaryChanged)
    Q_PROPERTY(QString outstandingText READ outstandingText NOTIFY summaryChanged)
    Q_PROPERTY(int     withBalanceCount READ withBalanceCount NOTIFY summaryChanged)
    Q_PROPERTY(int     atRiskCount     READ atRiskCount     NOTIFY summaryChanged)
    Q_PROPERTY(QString totalPaidText   READ totalPaidText   NOTIFY summaryChanged)   // settlement engine
    Q_PROPERTY(QString creditText      READ creditText      NOTIFY summaryChanged)   // settlement engine

public:
    explicit CustomersViewModel(CustomerListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int     totalCount()       const;
    int     filteredCount()    const;
    int     totalCustomers()   const;
    int     owingCount()       const;
    QString outstandingText()  const;
    int     withBalanceCount() const;
    int     atRiskCount()      const;
    QString totalPaidText()    const;
    QString creditText()       const;

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    CustomerListModel*   source_;
    CustomerFilterProxy* proxy_;

    int    totalCustomers_  = 0;
    int    withBalanceCount_ = 0;
    int    atRiskCount_     = 0;
    double outstandingSum_  = 0.0;
    double paidSum_         = 0.0;   // Σ payments (settlement engine)
    double creditSum_       = 0.0;   // Σ unallocated (settlement engine)
};

#endif // QUICK_CUSTOMERS_VIEW_MODEL_H
