#pragma once
#include <QMainWindow>

class NavigationPanel;
class GlobalToolBar;
class PageHeader;
class PageRouter;
class QStackedWidget;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void setupUi();
    void setupStatusBar();
    void connectSignals();
    void registerAllPages();

    NavigationPanel* m_navPanel;
    GlobalToolBar*   m_globalToolBar;
    PageHeader*      m_pageHeader;
    PageRouter*      m_router;
    QStackedWidget*  m_stack;

    QLabel* m_statusCompany;
    QLabel* m_statusPeriod;
    QLabel* m_statusRecordCount;
    QLabel* m_statusVersion;
};
