#include "pages/invoices/InvoicesPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "components/tables/PaginationProxy.h"
#include "models/InvoiceTableModel.h"
#include "dialogs/InvoiceEditorDialog.h"
#include "Invoice.h"
#include "storage/StorageService.h"
#include <QAction>
#include <QTabBar>
#include <QTableView>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <vector>

class InvoiceFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setStatusTab(const QString& tab) {
        m_status = (tab == "All") ? QString() : tab;
        invalidate();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_status.isEmpty()) {
            const QString s = sourceModel()->index(row, InvoiceTableModel::ColStatus, parent)
                                            .data().toString();
            if (s != m_status) return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }

private:
    QString m_status;
};

InvoicesPage::InvoicesPage(QWidget* parent)
    : ListPage(parent), m_statusTabs(nullptr), m_model(nullptr), m_proxy(nullptr)
{
    m_filterBar->addFilter("Customer", {});

    m_table->setEmptyMessage("No invoices yet",
                             "Create your first invoice using the New button.");
    m_table->setMoneyColumns({InvoiceTableModel::ColTotal});
    m_table->setStatusColumn(InvoiceTableModel::ColStatus);

    m_model = new InvoiceTableModel(this);
    auto* proxy = new InvoiceFilterProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    m_proxy = proxy;

    setupPagination(m_proxy);
    m_table->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());

    buildStatusTabs();
    setupListLayout();
    m_mainLayout->insertWidget(0, m_statusTabs);

    buildActions();
}

void InvoicesPage::loadFromStorage()
{
    if (!StorageService::instance().isInitialized()) return;
    auto rows = StorageService::instance().invoices().loadAll();
    m_model->setRows(std::move(rows));
}

void InvoicesPage::onSearch(const QString& text)
{
    m_proxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
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
    auto* newAct = new QAction("New", this);
    connect(newAct, &QAction::triggered, this, &InvoicesPage::onNewClicked);

    auto* editAct = new QAction("Edit", this);
    connect(editAct, &QAction::triggered, this, &InvoicesPage::onEditClicked);

    auto* voidAct = new QAction("Void", this);
    connect(voidAct, &QAction::triggered, this, &InvoicesPage::onVoidClicked);

    auto* refreshAct = new QAction("Refresh", this);
    connect(refreshAct, &QAction::triggered, this, &InvoicesPage::onRefreshClicked);

    m_actions = {
        newAct,
        editAct,
        voidAct,
        new QAction("Print", this),
        refreshAct,
        new QAction("Export", this),
    };
}

unsigned short int InvoicesPage::computeNextId() const
{
    unsigned short int maxId = 0;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const unsigned short int id = m_model->at(i).getId();
        if (id > maxId) maxId = id;
    }
    return static_cast<unsigned short int>(maxId + 1);
}

QString InvoicesPage::suggestNextNumber() const
{
    int maxNum = 1000;
    static const QRegularExpression numericTail("(\\d+)$");
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QString num = QString::fromUtf8(m_model->at(i).getInvoiceNumber());
        const auto m = numericTail.match(num);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            if (n > maxNum) maxNum = n;
        }
    }
    return QString("INV-%1").arg(maxNum + 1);
}

void InvoicesPage::onNewClicked()
{
    InvoiceEditorDialog dlg(this);
    dlg.setForAdd(computeNextId(), suggestNextNumber());
    if (dlg.exec() == QDialog::Accepted) {
        Invoice inv = dlg.invoice();
        if (StorageService::instance().isInitialized())
            StorageService::instance().invoices().save(inv);
        m_model->appendRow(inv);
        m_pagination->setTotalRecords(m_proxy->rowCount());
    }
}

void InvoicesPage::onEditClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (cur.isValid()) onRowDoubleClicked(cur.row());
}

void InvoicesPage::onVoidClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (!cur.isValid()) return;
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(cur)).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;
    Invoice inv = m_model->at(srcRow);
    if (inv.getStatus() == INVOICE_VOID) return;
    inv.setStatus(INVOICE_VOID);
    if (StorageService::instance().isInitialized())
        StorageService::instance().invoices().update(inv);
    m_model->updateRow(srcRow, inv);
}

void InvoicesPage::onRefreshClicked()
{
    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void InvoicesPage::onStatusTabChanged(int index)
{
    m_proxy->setStatusTab(m_statusTabs->tabText(index));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void InvoicesPage::onRowDoubleClicked(int proxyRow)
{
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(m_paginationProxy->index(proxyRow, 0))).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;

    InvoiceEditorDialog dlg(this);
    dlg.setForEdit(m_model->at(srcRow));
    if (dlg.exec() == QDialog::Accepted) {
        Invoice inv = dlg.invoice();
        if (StorageService::instance().isInitialized())
            StorageService::instance().invoices().update(inv);
        m_model->updateRow(srcRow, inv);
    }
}
