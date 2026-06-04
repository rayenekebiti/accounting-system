#include "pages/dashboard/DashboardPage.h"
#include "components/display/KpiCard.h"
#include "components/tables/DataTableView.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QHeaderView>
#include <QTableView>

DashboardPage::DashboardPage(QWidget* parent) : Page(parent)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* vLayout = new QVBoxLayout(content);
    vLayout->setContentsMargins(24, 24, 24, 24);
    vLayout->setSpacing(24);

    buildKpiRow(content);
    buildChartsRow(content);
    buildListsRow(content);

    for (auto* child : content->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
        vLayout->addWidget(child);
    }
    vLayout->addStretch();

    scroll->setWidget(content);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scroll);
}

void DashboardPage::buildKpiRow(QWidget* container)
{
    auto* row = new QWidget(container);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    m_receivablesCard = new KpiCard("Total Receivables", row);
    m_receivablesCard->setValue("$0.00");

    m_payablesCard = new KpiCard("Total Payables", row);
    m_payablesCard->setValue("$0.00");

    m_revenueCard = new KpiCard("Revenue (Period)", row);
    m_revenueCard->setValue("$0.00");

    m_overdueCard = new KpiCard("Overdue Invoices", row);
    m_overdueCard->setValue("0");

    layout->addWidget(m_receivablesCard, 1);
    layout->addWidget(m_payablesCard, 1);
    layout->addWidget(m_revenueCard, 1);
    layout->addWidget(m_overdueCard, 1);
}

void DashboardPage::buildChartsRow(QWidget* container)
{
    auto* row = new QWidget(container);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    layout->addWidget(makeChartCard("Revenue Trend"), 1);
    layout->addWidget(makeChartCard("Receivables Aging"), 1);
}

void DashboardPage::buildListsRow(QWidget* container)
{
    auto* row = new QWidget(container);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    auto buildCard = [&](const QString& title, DataTableView*& view, const QStringList& cols) -> QFrame* {
        auto* card = new QFrame(row);
        card->setObjectName("card");
        auto* vl = new QVBoxLayout(card);
        vl->setContentsMargins(16, 16, 16, 16);
        vl->setSpacing(8);

        auto* lbl = new QLabel(title, card);
        lbl->setObjectName("sectionTitle");
        vl->addWidget(lbl);

        view = new DataTableView(card);
        view->setColumns(cols);
        view->setCompactMode(true);
        view->setFixedHeight(220);
        view->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        vl->addWidget(view);
        return card;
    };

    m_recentInvoices  = nullptr;
    m_overdueInvoices = nullptr;

    layout->addWidget(buildCard("Recent Invoices",  m_recentInvoices,
                                {"#", "Customer", "Date", "Amount", "Status"}), 1);
    layout->addWidget(buildCard("Overdue Invoices", m_overdueInvoices,
                                {"#", "Customer", "Due Date", "Amount", "Days Over"}), 1);
}

QFrame* DashboardPage::makeChartCard(const QString& title)
{
    auto* card = new QFrame;
    card->setObjectName("card");
    card->setMinimumHeight(220);

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(16, 16, 16, 16);

    auto* lbl = new QLabel(title, card);
    lbl->setObjectName("sectionTitle");
    vl->addWidget(lbl);

    auto* placeholder = new QLabel("[ Chart Placeholder ]", card);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setObjectName("muted");
    placeholder->setStyleSheet("border: 1px dashed #E0E0E0; border-radius: 4px;");
    vl->addWidget(placeholder, 1);

    return card;
}

void DashboardPage::onActivated()
{
    m_receivablesCard->setValue("$24,850.00");
    m_receivablesCard->setDelta(12.5, true);
    m_payablesCard->setValue("$8,320.00");
    m_payablesCard->setDelta(-3.2, false);
    m_revenueCard->setValue("$61,400.00");
    m_revenueCard->setDelta(8.1, true);
    m_overdueCard->setValue("7");
    m_overdueCard->setSubtitle("$4,230 outstanding");
}
