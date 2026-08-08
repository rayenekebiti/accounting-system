#pragma once
#include "pages/base/Page.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QList>
#include <QStringList>

class QListWidget;
class QSplitter;
class QStackedWidget;
class QFrame;
class DataTableView;
class QComboBox;
class QPushButton;
class QDateEdit;
class QLabel;
class QStandardItemModel;

struct ReportOutput {
    QStringList        headers;
    QList<QStringList> rows;
    QList<int>         moneyCols;
    int                statusCol = -1;
    QString            error;
};

class ReportsPage : public Page {
    Q_OBJECT
public:
    explicit ReportsPage(QWidget* parent = nullptr);

    PageId  pageId()    const override { return PageId::Reports; }
    QString pageTitle() const override { return "Reports"; }

private slots:
    void onReportSelected(int row);
    void onRunClicked();
    void onReportFinished();

private:
    void   buildCatalog();
    void   buildParameterPanel(QWidget* parent);
    QFrame* makeChartPlaceholder();

    QString             currentReportName() const;
    void                endReport(const ReportOutput& out);

    QListWidget*        m_catalog;
    QStackedWidget*     m_results;
    DataTableView*      m_resultTable;
    QComboBox*          m_groupCombo;
    QPushButton*        m_runBtn;
    QPushButton*        m_exportBtn   = nullptr;
    QDateEdit*          m_fromDate;
    QDateEdit*          m_toDate;
    QLabel*             m_reportLabel;
    QStandardItemModel* m_reportModel = nullptr;
    QFutureWatcher<ReportOutput>* m_watcher = nullptr;
};
