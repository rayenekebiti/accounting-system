#ifndef QUICK_PAYMENTS_VIEW_MODEL_H
#define QUICK_PAYMENTS_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "PaymentListModel.h"
#include "PaymentFilterProxy.h"

class PaymentsViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel  READ listModel  CONSTANT)

    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(int     totalPayments      READ totalPayments      NOTIFY summaryChanged)
    Q_PROPERTY(QString totalReceivedText  READ totalReceivedText  NOTIFY summaryChanged)
    Q_PROPERTY(QString unallocatedText    READ unallocatedText    NOTIFY summaryChanged)
    Q_PROPERTY(int     unallocatedCount   READ unallocatedCount   NOTIFY summaryChanged)

public:
    explicit PaymentsViewModel(PaymentListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int     totalCount()      const;
    int     filteredCount()   const;
    int     totalPayments()   const;
    QString totalReceivedText() const;
    QString unallocatedText() const;
    int     unallocatedCount() const;

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    PaymentListModel*   source_;
    PaymentFilterProxy* proxy_;

    int     totalPayments_    = 0;
    int     unallocatedCount_ = 0;
    int64_t receivedSum_      = 0;
    int64_t unallocatedSum_   = 0;
};

#endif // QUICK_PAYMENTS_VIEW_MODEL_H
