#include "pages/customers/CustomersPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include <QAction>

CustomersPage::CustomersPage(QWidget* parent) : ListPage(parent)
{
    m_filterBar->addFilter("Status", {"Active", "Inactive"});

    m_table->setColumns({"Name", "Email", "Phone", "Balance", "Status"});
    m_table->setEmptyMessage("No customers yet",
                             "Add your first customer to get started.");

    m_pagination->setTotalRecords(0);

    setupListLayout();
    buildActions();
}

void CustomersPage::buildActions()
{
    auto* addAct      = new QAction("Add", this);
    auto* editAct     = new QAction("Edit", this);
    auto* deactivAct  = new QAction("Deactivate", this);
    auto* refreshAct  = new QAction("Refresh", this);
    auto* exportAct   = new QAction("Export", this);

    m_actions = { addAct, editAct, deactivAct, refreshAct, exportAct };
}

void CustomersPage::onRowDoubleClicked(int row)
{
    Q_UNUSED(row)
    // Opens CustomerEditorDialog — placeholder
}
