#include "pages/invoices/InvoicesPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include <QAction>
#include <QTabBar>
#include <QVBoxLayout>

InvoicesPage::InvoicesPage(QWidget* parent) : ListPage(parent)
{
    m_filterBar->addFilter("Customer", {});

    m_table->setColumns({"Number", "Customer", "Date", "Due", "Total", "Status"});
    m_table->setEmptyMessage("No invoices yet",
                             "Create your first invoice using the New button.");
    m_table->setMoneyColumns({4});
    m_table->setStatusColumn(5);
    m_table->enableRowActions();

    m_pagination->setTotalRecords(0);

    buildStatusTabs();
    setupListLayout();

    // Insert tabs above the filter strip
    m_mainLayout->insertWidget(0, m_statusTabs);

    buildActions();
}

void InvoicesPage::buildStatusTabs()
{
    m_statusTabs = new QTabBar(this);
    m_statusTabs->setExpanding(false);
    for (const QString& tab : {"All", "Draft", "Posted", "Overdue", "Paid"})
        m_statusTabs->addTab(tab);

    connect(m_statusTabs, &QTabBar::currentChanged,
            this, &InvoicesPage::onStatusTabChanged);
}

void InvoicesPage::buildActions()
{
    m_actions = {
        new QAction("New", this),
        new QAction("Edit", this),
        new QAction("Void", this),
        new QAction("Print", this),
        new QAction("Refresh", this),
        new QAction("Export", this),
    };
}

void InvoicesPage::onStatusTabChanged(int index)
{
    Q_UNUSED(index)
    // Filter by status tab — presenter would be called here
}

void InvoicesPage::onRowDoubleClicked(int row)
{
    Q_UNUSED(row)
    // Opens InvoiceEditorDialog (modeless) — placeholder
}
