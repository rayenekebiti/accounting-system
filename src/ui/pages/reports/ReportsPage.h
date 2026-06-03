#pragma once
#include "pages/base/Page.h"

class QListWidget;
class QSplitter;
class QStackedWidget;
class QFrame;
class DataTableView;
class QComboBox;
class QPushButton;

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
    void buildCatalog();
    void buildParameterPanel(QWidget* parent);
    QFrame* makeChartPlaceholder();

    QListWidget*   m_catalog;
    QStackedWidget* m_results;
    DataTableView*  m_resultTable;
    QComboBox*      m_groupCombo;
    QPushButton*    m_runBtn;
};
