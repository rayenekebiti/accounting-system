#ifndef QUICK_INVOICES_VIEW_MODEL_H
#define QUICK_INVOICES_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "InvoiceListModel.h"
#include "InvoiceFilterProxy.h"

class InvoicesViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel  READ listModel  CONSTANT)

    // Explicit counts drive the screen's state machine. QAbstractItemModel has
    // no QML `count` property, so the UI must NOT rely on listModel.count —
    // these are the robust source of truth.
    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(QString outstandingText  READ outstandingText  NOTIFY summaryChanged)
    Q_PROPERTY(int     outstandingCount READ outstandingCount NOTIFY summaryChanged)
    Q_PROPERTY(QString overdueText      READ overdueText      NOTIFY summaryChanged)
    Q_PROPERTY(int     overdueCount     READ overdueCount     NOTIFY summaryChanged)
    Q_PROPERTY(QString paidText         READ paidText         NOTIFY summaryChanged)
    Q_PROPERTY(int     paidCount        READ paidCount        NOTIFY summaryChanged)
    Q_PROPERTY(int     awaitingCount    READ awaitingCount    NOTIFY summaryChanged)
    Q_PROPERTY(int     draftCount       READ draftCount       NOTIFY summaryChanged)

public:
    explicit InvoicesViewModel(InvoiceListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int     totalCount()       const;
    int     filteredCount()    const;
    QString outstandingText()  const;
    int     outstandingCount() const;
    QString overdueText()      const;
    int     overdueCount()     const;
    QString paidText()         const;
    int     paidCount()        const;
    int     awaitingCount()    const;
    int     draftCount()       const;

    Q_INVOKABLE void setStatusFilter(const QString& status);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filterChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    InvoiceListModel*    source_;
    InvoiceFilterProxy*  proxy_;

    double outstandingSum_  = 0.0;
    int    outstandingCount_ = 0;
    double overdueSum_      = 0.0;
    int    overdueCount_    = 0;
    double paidSum_         = 0.0;
    int    paidCount_       = 0;
    int    totalCount_      = 0;
    int    awaitingCount_   = 0;   // Posted + Overdue
    int    draftCount_      = 0;
};

#endif // QUICK_INVOICES_VIEW_MODEL_H
