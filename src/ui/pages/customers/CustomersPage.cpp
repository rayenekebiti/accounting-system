#include "pages/customers/CustomersPage.h"
#include "components/inputs/SearchBar.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "models/CustomerTableModel.h"
#include "dialogs/CustomerEditorDialog.h"
#include "Customer.h"
#include "storage/StorageService.h"
#include "components/tables/PaginationProxy.h"
#include <QAction>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <vector>

class CustomerFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setStatusFilter(const QString& value) { m_status = value; invalidate(); }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_status.isEmpty()) {
            const QString s = sourceModel()
                ->index(row, CustomerTableModel::ColStatus, parent).data().toString();
            if (s != m_status) return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
private:
    QString m_status;
};

CustomersPage::CustomersPage(QWidget* parent)
    : ListPage(parent), m_model(nullptr), m_proxy(nullptr)
{
    m_filterBar->addFilter("Status", {"Active", "Inactive"});

    m_table->setEmptyMessage("No customers yet",
                             "Add your first customer to get started.");
    m_table->setMoneyColumns({CustomerTableModel::ColBalance});
    m_table->setStatusColumn(CustomerTableModel::ColStatus);

    m_model = new CustomerTableModel(this);
    auto* proxy = new CustomerFilterProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    m_proxy = proxy;

    setupPagination(m_proxy);
    m_table->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());

    connect(m_filterBar, &FilterBar::filterChanged, this, &CustomersPage::rebuildFilter);

    setupListLayout();
    buildActions();
}

void CustomersPage::loadFromStorage()
{
    if (!StorageService::instance().isInitialized()) return;
    m_model->setRows(StorageService::instance().customers().loadAll());
}

void CustomersPage::onSearch(const QString& text)
{
    m_proxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void CustomersPage::rebuildFilter()
{
    static_cast<CustomerFilterProxy*>(m_proxy)->setStatusFilter(
        m_filterBar->filterValue(0));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void CustomersPage::buildActions()
{
    auto* addAct = new QAction("Add", this);
    connect(addAct, &QAction::triggered, this, &CustomersPage::onAddClicked);

    auto* editAct = new QAction("Edit", this);
    connect(editAct, &QAction::triggered, this, &CustomersPage::onEditClicked);

    auto* deactAct = new QAction("Deactivate", this);
    connect(deactAct, &QAction::triggered, this, &CustomersPage::onDeactivateClicked);

    auto* refreshAct = new QAction("Refresh", this);
    connect(refreshAct, &QAction::triggered, this, &CustomersPage::onRefreshClicked);

    m_actions = { addAct, editAct, deactAct, refreshAct, new QAction("Export", this) };
}

unsigned short int CustomersPage::computeNextId() const
{
    unsigned short int maxId = 1000;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const unsigned short int id = m_model->at(i).getId();
        if (id > maxId) maxId = id;
    }
    return static_cast<unsigned short int>(maxId + 1);
}

void CustomersPage::onAddClicked()
{
    CustomerEditorDialog dlg(this);
    dlg.setForAdd(computeNextId());
    if (dlg.exec() == QDialog::Accepted) {
        Customer cust = dlg.customer();
        if (StorageService::instance().isInitialized())
            StorageService::instance().customers().save(cust);
        m_model->appendRow(cust);
        m_pagination->setTotalRecords(m_proxy->rowCount());
    }
}

void CustomersPage::onEditClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (cur.isValid()) onRowDoubleClicked(cur.row());
}

void CustomersPage::onDeactivateClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (!cur.isValid()) return;
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(cur)).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;
    Customer cust = m_model->at(srcRow);
    cust.setIsDeleted(true);
    if (StorageService::instance().isInitialized())
        StorageService::instance().customers().update(cust);
    m_model->removeRow(srcRow);
    m_pagination->setTotalRecords(m_proxy->rowCount());
}

void CustomersPage::onRefreshClicked()
{
    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void CustomersPage::onRowDoubleClicked(int proxyRow)
{
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(m_paginationProxy->index(proxyRow, 0))).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;

    CustomerEditorDialog dlg(this);
    dlg.setForEdit(m_model->at(srcRow));
    if (dlg.exec() == QDialog::Accepted) {
        Customer cust = dlg.customer();
        if (StorageService::instance().isInitialized())
            StorageService::instance().customers().update(cust);
        m_model->updateRow(srcRow, cust);
    }
}
