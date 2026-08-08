#ifndef QUICK_EXPENSES_VIEW_MODEL_H
#define QUICK_EXPENSES_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "ExpenseListModel.h"
#include "ExpenseFilterProxy.h"

class ExpensesViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* listModel  READ listModel  CONSTANT)

    Q_PROPERTY(int totalCount    READ totalCount    NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)

    Q_PROPERTY(int     expenseCount   READ expenseCount   NOTIFY summaryChanged)
    Q_PROPERTY(QString totalSpentText READ totalSpentText NOTIFY summaryChanged)
    Q_PROPERTY(QString cashText       READ cashText       NOTIFY summaryChanged)
    Q_PROPERTY(QString creditText     READ creditText     NOTIFY summaryChanged)
    Q_PROPERTY(int     voidCount      READ voidCount      NOTIFY summaryChanged)

public:
    explicit ExpensesViewModel(ExpenseListModel* source, QObject* parent = nullptr);

    QAbstractItemModel* listModel() const;

    int     totalCount()    const;
    int     filteredCount() const;
    int     expenseCount()  const;
    QString totalSpentText() const;
    QString cashText()      const;
    QString creditText()    const;
    int     voidCount()     const;

    Q_INVOKABLE void setCategoryFilter(const QString& category);
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void refresh();

signals:
    void summaryChanged();
    void filteredCountChanged();

private:
    void recomputeSummaries();

    ExpenseListModel*   source_;
    ExpenseFilterProxy* proxy_;

    int     expenseCount_ = 0;
    int     voidCount_    = 0;
    int64_t netSpent_     = 0;
    int64_t cashSpent_    = 0;
    int64_t creditSpent_  = 0;
};

#endif // QUICK_EXPENSES_VIEW_MODEL_H
