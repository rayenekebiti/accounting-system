#include "pages/payments/PaymentsPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include <QAction>

PaymentsPage::PaymentsPage(QWidget* parent) : ListPage(parent)
{
    m_filterBar->addFilter("Method", {"Cash", "Bank Transfer", "Card", "Cheque"});
    m_filterBar->addFilter("Party", {});

    m_table->setColumns({"Date", "Party", "Method", "Amount", "Allocated", "Reference"});
    m_table->setEmptyMessage("No payments recorded",
                             "Receive a payment to track allocations.");

    m_pagination->setTotalRecords(0);

    setupListLayout();
    buildActions();
}

void PaymentsPage::buildActions()
{
    m_actions = {
        new QAction("Receive", this),
        new QAction("Edit", this),
        new QAction("Refresh", this),
        new QAction("Export", this),
    };
}

void PaymentsPage::onRowDoubleClicked(int row)
{
    Q_UNUSED(row)
    // Opens ReceivePaymentDialog (modeless) — placeholder
}
