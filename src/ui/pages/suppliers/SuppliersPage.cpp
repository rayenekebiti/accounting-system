#include "pages/suppliers/SuppliersPage.h"
#include "components/inputs/SearchBar.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "models/SupplierTableModel.h"
#include "dialogs/SupplierEditorDialog.h"
#include "Supplier.h"
#include "storage/StorageService.h"
#include "components/tables/PaginationProxy.h"
#include <QAction>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <vector>

class SupplierFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setStatusFilter(const QString& value) { m_status = value; invalidate(); }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_status.isEmpty()) {
            const QString s = sourceModel()
                ->index(row, SupplierTableModel::ColStatus, parent).data().toString();
            if (s != m_status) return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
private:
    QString m_status;
};

SuppliersPage::SuppliersPage(QWidget* parent)
    : ListPage(parent), m_model(nullptr), m_proxy(nullptr)
{
    m_filterBar->addFilter("Status", {"Active", "Inactive"});

    m_table->setEmptyMessage("No suppliers yet",
                             "Add your first supplier to track payables.");
    m_table->setMoneyColumns({SupplierTableModel::ColBalance});
    m_table->setStatusColumn(SupplierTableModel::ColStatus);

    m_model = new SupplierTableModel(this);
    auto* proxy = new SupplierFilterProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    m_proxy = proxy;

    setupPagination(m_proxy);
    m_table->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());

    connect(m_filterBar, &FilterBar::filterChanged, this, &SuppliersPage::rebuildFilter);

    setupListLayout();
    buildActions();
}

void SuppliersPage::loadFromStorage()
{
    if (!StorageService::instance().isInitialized()) return;
    m_model->setRows(StorageService::instance().suppliers().loadAll());
}

void SuppliersPage::onSearch(const QString& text)
{
    m_proxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void SuppliersPage::rebuildFilter()
{
    static_cast<SupplierFilterProxy*>(m_proxy)->setStatusFilter(
        m_filterBar->filterValue(0));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void SuppliersPage::buildActions()
{
    auto* addAct = new QAction("Add", this);
    connect(addAct, &QAction::triggered, this, &SuppliersPage::onAddClicked);

    auto* editAct = new QAction("Edit", this);
    connect(editAct, &QAction::triggered, this, &SuppliersPage::onEditClicked);

    auto* deactAct = new QAction("Deactivate", this);
    connect(deactAct, &QAction::triggered, this, &SuppliersPage::onDeactivateClicked);

    auto* refreshAct = new QAction("Refresh", this);
    connect(refreshAct, &QAction::triggered, this, &SuppliersPage::onRefreshClicked);

    m_actions = { addAct, editAct, deactAct, refreshAct, new QAction("Export", this) };
}

unsigned short int SuppliersPage::computeNextId() const
{
    unsigned short int maxId = 2000;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const unsigned short int id = m_model->at(i).getId();
        if (id > maxId) maxId = id;
    }
    return static_cast<unsigned short int>(maxId + 1);
}

void SuppliersPage::onAddClicked()
{
    SupplierEditorDialog dlg(this);
    dlg.setForAdd(computeNextId());
    if (dlg.exec() == QDialog::Accepted) {
        Supplier supp = dlg.supplier();
        if (StorageService::instance().isInitialized())
            StorageService::instance().suppliers().save(supp);
        m_model->appendRow(supp);
        m_pagination->setTotalRecords(m_proxy->rowCount());
    }
}

void SuppliersPage::onEditClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (cur.isValid()) onRowDoubleClicked(cur.row());
}

void SuppliersPage::onDeactivateClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (!cur.isValid()) return;
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(cur)).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;
    Supplier supp = m_model->at(srcRow);
    supp.setIsDeleted(true);
    if (StorageService::instance().isInitialized())
        StorageService::instance().suppliers().update(supp);
    m_model->removeRow(srcRow);
    m_pagination->setTotalRecords(m_proxy->rowCount());
}

void SuppliersPage::onRefreshClicked()
{
    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void SuppliersPage::onRowDoubleClicked(int proxyRow)
{
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(m_paginationProxy->index(proxyRow, 0))).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;

    SupplierEditorDialog dlg(this);
    dlg.setForEdit(m_model->at(srcRow));
    if (dlg.exec() == QDialog::Accepted) {
        Supplier supp = dlg.supplier();
        if (StorageService::instance().isInitialized())
            StorageService::instance().suppliers().update(supp);
        m_model->updateRow(srcRow, supp);
    }
}
