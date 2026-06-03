#include "pages/suppliers/SuppliersPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include <QAction>

SuppliersPage::SuppliersPage(QWidget* parent) : ListPage(parent)
{
    m_filterBar->addFilter("Status", {"Active", "Inactive"});

    m_table->setColumns({"Name", "Contact", "Email", "Outstanding", "Status"});
    m_table->setEmptyMessage("No suppliers yet",
                             "Add your first supplier to track payables.");

    m_pagination->setTotalRecords(0);

    setupListLayout();
    buildActions();
}

void SuppliersPage::buildActions()
{
    m_actions = {
        new QAction("Add", this),
        new QAction("Edit", this),
        new QAction("Deactivate", this),
        new QAction("Refresh", this),
        new QAction("Export", this),
    };
}

void SuppliersPage::onRowDoubleClicked(int row)
{
    Q_UNUSED(row)
    // Opens SupplierEditorDialog — placeholder
}
