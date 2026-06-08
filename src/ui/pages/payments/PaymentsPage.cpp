#include "pages/payments/PaymentsPage.h"
#include "components/inputs/FilterBar.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "components/tables/PaginationProxy.h"
#include "models/PaymentTableModel.h"
#include "dialogs/PaymentEditorDialog.h"
#include "Payment.h"
#include "storage/StorageService.h"
#include <QAction>
#include <QMessageBox>
#include <QTableView>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QRegularExpression>
#include <vector>

class PaymentFilterProxy : public QSortFilterProxyModel {
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setMethodFilter(const QString& value) { m_method = value; invalidate(); }
    void setPartyFilter(const QString& value)  { m_party  = value; invalidate(); }

protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (!m_method.isEmpty()) {
            const QString m = sourceModel()
                ->index(row, PaymentTableModel::ColMethod, parent).data().toString();
            if (m != m_method) return false;
        }
        if (!m_party.isEmpty()) {
            const QString p = sourceModel()
                ->index(row, PaymentTableModel::ColParty, parent).data().toString();
            if (!p.startsWith(m_party)) return false;
        }
        return QSortFilterProxyModel::filterAcceptsRow(row, parent);
    }

private:
    QString m_method;
    QString m_party;
};

PaymentsPage::PaymentsPage(QWidget* parent)
    : ListPage(parent), m_model(nullptr), m_proxy(nullptr)
{
    m_filterBar->addFilter("Method", {"Cash", "Bank", "Check", "Card"});
    m_filterBar->addFilter("Party",  {"Customer", "Supplier"});

    m_table->setEmptyMessage("No payments recorded",
                             "Receive a payment to track allocations.");
    m_table->setMoneyColumns({PaymentTableModel::ColAmount});

    m_model = new PaymentTableModel(this);
    auto* proxy = new PaymentFilterProxy(this);
    proxy->setSourceModel(m_model);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1);
    m_proxy = proxy;

    setupPagination(m_proxy);
    m_table->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());

    connect(m_filterBar, &FilterBar::filterChanged, this, &PaymentsPage::rebuildFilter);

    setupListLayout();
    buildActions();
}

void PaymentsPage::loadFromStorage()
{
    if (!StorageService::instance().isInitialized()) return;
    try {
        m_model->setRows(StorageService::instance().payments().loadAll());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load Error", QString::fromUtf8(e.what()));
    }
}

void PaymentsPage::onSearch(const QString& text)
{
    m_proxy->setFilterRegularExpression(
        QRegularExpression(QRegularExpression::escape(text),
                           QRegularExpression::CaseInsensitiveOption));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void PaymentsPage::rebuildFilter()
{
    auto* proxy = static_cast<PaymentFilterProxy*>(m_proxy);
    proxy->setMethodFilter(m_filterBar->filterValue(0));
    proxy->setPartyFilter(m_filterBar->filterValue(1));
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void PaymentsPage::buildActions()
{
    auto* receiveAct = new QAction("Receive", this);
    connect(receiveAct, &QAction::triggered, this, &PaymentsPage::onAddClicked);

    auto* editAct = new QAction("Edit", this);
    connect(editAct, &QAction::triggered, this, &PaymentsPage::onEditClicked);

    auto* refreshAct = new QAction("Refresh", this);
    connect(refreshAct, &QAction::triggered, this, &PaymentsPage::onRefreshClicked);

    auto* exportAct = new QAction("Export", this);
    exportAct->setEnabled(false);
    exportAct->setToolTip("Export coming in a future release");

    m_actions = { receiveAct, editAct, refreshAct, exportAct };
}

unsigned short int PaymentsPage::computeNextId() const
{
    unsigned short int maxId = 0;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const unsigned short int id = m_model->at(i).getId();
        if (id > maxId) maxId = id;
    }
    return static_cast<unsigned short int>(maxId + 1);
}

QString PaymentsPage::suggestNextNumber() const
{
    int maxNum = 0;
    static const QRegularExpression numericTail("(\\d+)$");
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const QString num = QString::fromUtf8(m_model->at(i).getPaymentNumber());
        const auto m = numericTail.match(num);
        if (m.hasMatch()) {
            const int n = m.captured(1).toInt();
            if (n > maxNum) maxNum = n;
        }
    }
    return QString("PMT-%1").arg(maxNum + 1, 4, 10, QChar('0'));
}

void PaymentsPage::onAddClicked()
{
    PaymentEditorDialog dlg(this);
    dlg.setForAdd(computeNextId(), suggestNextNumber());
    if (dlg.exec() == QDialog::Accepted) {
        Payment pay = dlg.payment();
        if (StorageService::instance().isInitialized()) {
            try {
                StorageService::instance().payments().save(pay);
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Save Error", QString::fromUtf8(e.what()));
                return;
            }
        }
        m_model->appendRow(pay);
        m_pagination->setTotalRecords(m_proxy->rowCount());
    }
}

void PaymentsPage::onEditClicked()
{
    const QModelIndex cur = m_table->tableView()->selectionModel()->currentIndex();
    if (cur.isValid()) onRowDoubleClicked(cur.row());
}

void PaymentsPage::onRefreshClicked()
{
    loadFromStorage();
    m_pagination->setTotalRecords(m_proxy->rowCount());
    resetToFirstPage();
}

void PaymentsPage::onRowDoubleClicked(int proxyRow)
{
    const int srcRow = m_filterProxy->mapToSource(
        m_paginationProxy->mapToSource(m_paginationProxy->index(proxyRow, 0))).row();
    if (srcRow < 0 || srcRow >= m_model->rowCount()) return;

    PaymentEditorDialog dlg(this);
    dlg.setForEdit(m_model->at(srcRow));
    if (dlg.exec() == QDialog::Accepted) {
        Payment pay = dlg.payment();
        if (StorageService::instance().isInitialized()) {
            try {
                StorageService::instance().payments().update(pay);
            } catch (const std::exception& e) {
                QMessageBox::critical(this, "Save Error", QString::fromUtf8(e.what()));
                return;
            }
        }
        m_model->updateRow(srcRow, pay);
    }
}
