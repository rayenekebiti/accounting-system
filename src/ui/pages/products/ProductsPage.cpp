#include "pages/products/ProductsPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include <QAction>

ProductsPage::ProductsPage(QWidget* parent) : ListPage(parent)
{
    m_filterBar->addFilter("Category", {"Goods", "Services"});
    m_filterBar->addFilter("Stock", {"In Stock", "Low", "Out of Stock"});

    m_table->setColumns({"Name", "SKU", "Price", "Stock", "Status"});
    m_table->setEmptyMessage("No products yet",
                             "Add products to include them on invoices.");

    m_pagination->setTotalRecords(0);

    setupListLayout();
    buildActions();
}

void ProductsPage::buildActions()
{
    m_actions = {
        new QAction("Add", this),
        new QAction("Edit", this),
        new QAction("Adjust Stock", this),
        new QAction("Refresh", this),
        new QAction("Export", this),
    };
}

void ProductsPage::onRowDoubleClicked(int row)
{
    Q_UNUSED(row)
    // Opens ProductEditorDialog — placeholder
}
