#include "pages/products/ProductsPage.h"
#include "components/inputs/SearchBar.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "models/ProductTableModel.h"
#include "dialogs/ProductEditorDialog.h"
#include "services/Exporter.h"
#include "Product.h"
#include "storage/StorageService.h"
#include "components/tables/PaginationProxy.h"
#include <QAction>
#include <QMessageBox>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <vector>

class ProductFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;
    void setCategoryFilter(const QString& v) { m_category = v; invalidate(); }
    void setStockFilter(const QString& v)    { m_stock    = v; invalidate(); }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_category.isEmpty()) {
            const QString code = sourceModel()
                ->index(row, ProductTableModel::ColCode, parent).data().toString();
            const bool isSvc = code.startsWith("SVC", Qt::CaseInsensitive);
            if (m_category == "Services" && !isSvc) return false;
            if (m_category == "Goods"    &&  isSvc) return false;
        }
        if (!m_stock.isEmpty()) {
            const QString s = sourceModel()
                ->index(row, ProductTableModel::ColStatus, parent).data().toString();
            if (s != m_stock) return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }
private:
    QString m_category;
    QString m_stock;
};

ProductsPage::ProductsPage(QWidget* parent)
    : ListPage(parent), m_model(nullptr), m_proxy(nullptr)
{
    m_filterBar->addFilter("Category", {"Goods", "Services"});
    m_filterBar->addFilter("Stock",    {"In Stock", "Low", "Out"});

    m_table->setEmptyMessage("No products yet",
                             "Add products to include them on invoices.");
    m_table->setMoneyColumns({ProductTableModel::ColPrice, ProductTableModel::ColCost});
    m_table->setStatusColumn(ProductTableModel::ColStatus);

    m_model = new ProductTableModel(this);
    auto* proxy = new ProductFilterProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    m_proxy = proxy;

    setupPagination(m_proxy);
    m_table->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());

    connect(m_filterBar, &FilterBar::filterChanged, this, &ProductsPage::rebuildFilter);

    setupListLayout();
    buildActions();
}

void ProductsPage::loadFromStorage()
{
    if (!StorageService::instance().isInitialized()) return;
    try {
        m_model->setRows(StorageService::instance().products().loadAll());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load Error", QString::fromUtf8(e.what()));
    }
}

void ProductsPage::onSearch(const QString& text)
{
    m_proxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void ProductsPage::rebuildFilter()
{
    auto* proxy = static_cast<ProductFilterProxy*>(m_proxy);
    proxy->setCategoryFilter(m_filterBar->filterValue(0));
    proxy->setStockFilter(m_filterBar->filterValue(1));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void ProductsPage::buildActions()
{
    auto* addAct = new QAction("Add", this);
    connect(addAct, &QAction::triggered, this, &ProductsPage::onAddClicked);

    auto* editAct = new QAction("Edit", this);
    connect(editAct, &QAction::triggered, this, &ProductsPage::onEditClicked);

    auto* refreshAct = new QAction("Refresh", this);
    connect(refreshAct, &QAction::triggered, this, &ProductsPage::onRefreshClicked);

    auto* deactAct = new QAction("Deactivate", this);
    connect(deactAct, &QAction::triggered, this, &ProductsPage::onDeactivateClicked);

    auto* adjustAct = new QAction("Adjust Stock", this);
    adjustAct->setEnabled(false);
    adjustAct->setToolTip("Stock adjustments coming in a future release");

    auto* exportAct = new QAction("Export", this);
    connect(exportAct, &QAction::triggered, this, [this] {
        Exporter::toCsv(m_proxy, this);
    });

    m_actions = { addAct, editAct, deactAct, adjustAct, refreshAct, exportAct };
}

uint32_t ProductsPage::computeNextId() const
{
    uint32_t maxId = 0;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const uint32_t id = m_model->at(i).getId();
        if (id > maxId) maxId = id;
    }
    return maxId + 1;
}

void ProductsPage::onAddClicked()
{
    ProductEditorDialog dlg(this);
    dlg.setForAdd(computeNextId());
    if (dlg.exec() == QDialog::Accepted) {
        Product prod = dlg.product();
        if (StorageService::instance().isInitialized()) {
            try {
                StorageService::instance().products().save(prod);
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Save Error", QString::fromUtf8(e.what()));
                return;
            }
        }
        m_model->appendRow(prod);
        m_pagination->setTotalRecords(m_proxy->rowCount());
    }
}

void ProductsPage::onEditClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (cur.isValid()) onRowDoubleClicked(cur.row());
}

void ProductsPage::onDeactivateClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (!cur.isValid()) return;
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(cur)).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;
    Product prod = m_model->at(srcRow);
    prod.setIsDeleted(true);
    if (StorageService::instance().isInitialized()) {
        try {
            StorageService::instance().products().update(prod);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, "Update Error", QString::fromUtf8(e.what()));
            return;
        }
    }
    m_model->removeRow(srcRow);
    m_pagination->setTotalRecords(m_proxy->rowCount());
}

void ProductsPage::onRefreshClicked()
{
    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void ProductsPage::onRowDoubleClicked(int proxyRow)
{
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(m_paginationProxy->index(proxyRow, 0))).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;

    ProductEditorDialog dlg(this);
    dlg.setForEdit(m_model->at(srcRow));
    if (dlg.exec() == QDialog::Accepted) {
        Product prod = dlg.product();
        if (StorageService::instance().isInitialized()) {
            try {
                StorageService::instance().products().update(prod);
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Save Error", QString::fromUtf8(e.what()));
                return;
            }
        }
        m_model->updateRow(srcRow, prod);
    }
}
