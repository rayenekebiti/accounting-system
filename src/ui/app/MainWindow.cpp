#include "app/MainWindow.h"
#include "app/NavigationPanel.h"
#include "app/GlobalToolBar.h"
#include "app/PageHeader.h"
#include "app/PageRouter.h"
#include "pages/dashboard/DashboardPage.h"
#include "pages/customers/CustomersPage.h"
#include "pages/suppliers/SuppliersPage.h"
#include "pages/products/ProductsPage.h"
#include "pages/invoices/InvoicesPage.h"
#include "pages/payments/PaymentsPage.h"
#include "pages/reports/ReportsPage.h"
#include "pages/settings/SettingsPage.h"
#include <QStackedWidget>
#include <QStatusBar>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle("AccountingPro");
    setMinimumSize(1024, 640);
    resize(1280, 800);

    setupUi();
    setupStatusBar();
    registerAllPages();
    connectSignals();

    m_router->navigateTo(PageId::Dashboard);
}

void MainWindow::setupUi()
{
    m_globalToolBar = new GlobalToolBar(this);
    addToolBar(Qt::TopToolBarArea, m_globalToolBar);

    // Central widget: nav panel + content area
    auto* central  = new QWidget(this);
    auto* hLayout  = new QHBoxLayout(central);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    setCentralWidget(central);

    m_navPanel = new NavigationPanel(central);
    hLayout->addWidget(m_navPanel);

    // Right side: header + stack
    auto* contentArea = new QWidget(central);
    auto* vLayout     = new QVBoxLayout(contentArea);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    m_pageHeader = new PageHeader(contentArea);
    vLayout->addWidget(m_pageHeader);

    m_stack = new QStackedWidget(contentArea);
    vLayout->addWidget(m_stack, 1);

    hLayout->addWidget(contentArea, 1);

    m_router = new PageRouter(m_stack, m_pageHeader, this);
}

void MainWindow::setupStatusBar()
{
    auto* sb = statusBar();
    sb->showMessage("Ready");

    m_statusCompany     = new QLabel("Acme Ltd", this);
    m_statusPeriod      = new QLabel("FY 2026", this);
    m_statusRecordCount = new QLabel("", this);
    m_statusVersion     = new QLabel("v1.0", this);

    for (auto* lbl : {m_statusCompany, m_statusPeriod, m_statusRecordCount, m_statusVersion}) {
        lbl->setObjectName("muted");
        lbl->setContentsMargins(8, 0, 8, 0);
        sb->addPermanentWidget(lbl);
    }
}

void MainWindow::registerAllPages()
{
    m_router->registerPage(new DashboardPage(m_stack));
    m_router->registerPage(new CustomersPage(m_stack));
    m_router->registerPage(new SuppliersPage(m_stack));
    m_router->registerPage(new ProductsPage(m_stack));
    m_router->registerPage(new InvoicesPage(m_stack));
    m_router->registerPage(new PaymentsPage(m_stack));
    m_router->registerPage(new ReportsPage(m_stack));
    m_router->registerPage(new SettingsPage(m_stack));
}

void MainWindow::connectSignals()
{
    connect(m_navPanel, &NavigationPanel::navigationRequested,
            m_router,   &PageRouter::navigateTo);

    connect(m_router, &PageRouter::pageActivated,
            m_navPanel, &NavigationPanel::setActivePage);

    connect(m_globalToolBar, &GlobalToolBar::newInvoiceRequested, this, [this] {
        m_router->navigateTo(PageId::Invoices);
        statusBar()->showMessage("New invoice…", 3000);
    });
    connect(m_globalToolBar, &GlobalToolBar::newPaymentRequested, this, [this] {
        m_router->navigateTo(PageId::Payments);
        statusBar()->showMessage("New payment…", 3000);
    });
    connect(m_globalToolBar, &GlobalToolBar::newCustomerRequested, this, [this] {
        m_router->navigateTo(PageId::Customers);
        statusBar()->showMessage("New customer…", 3000);
    });
    connect(m_globalToolBar, &GlobalToolBar::periodChanged, this, [this](const QString& p) {
        m_statusPeriod->setText(p);
    });
}
