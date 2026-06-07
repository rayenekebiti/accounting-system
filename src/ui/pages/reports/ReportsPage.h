#pragma once
#include "pages/base/Page.h"

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

class ReportsPage : public Page {
    Q_OBJECT
public:
    explicit ReportsPage(QWidget* parent = nullptr);

    PageId  pageId()    const override { return PageId::Reports; }
    QString pageTitle() const override { return "Reports"; }

private slots:
    void onReportSelected(int row);
    void onRunClicked();

private:
    void   buildCatalog();
    void   buildParameterPanel(QWidget* parent);
    QFrame* makeChartPlaceholder();

    void runAgedReceivables();
    void runCustomerStatement();
    void runAgedPayables();
    void runSupplierStatement();
    void runSalesSummary();
    void runInvoiceRegister();
    void runTaxSummary();
    void runVATReturn();
    void runProfitAndLoss();
    void runCashFlow();

    QString             currentReportName() const;
    QStandardItemModel* beginReport(const QStringList& headers);
    void                endReport(QStandardItemModel* model,
                                  const QList<int>& moneyCols = {},
                                  int statusCol = -1);

    QListWidget*        m_catalog;
    QStackedWidget*     m_results;
    DataTableView*      m_resultTable;
    QComboBox*          m_groupCombo;
    QPushButton*        m_runBtn;
    QDateEdit*          m_fromDate;
    QDateEdit*          m_toDate;
    QLabel*             m_reportLabel;
    QStandardItemModel* m_reportModel = nullptr;
};
