#pragma once
#include "pages/base/ListPage.h"

class QTabBar;
class InvoiceTableModel;

class InvoicesPage : public ListPage {
    Q_OBJECT
public:
    explicit InvoicesPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Invoices; }
    QString         pageTitle()   const override { return "Invoices"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private slots:
    void onStatusTabChanged(int index);
    void onNewClicked();
    void onEditClicked();
    void onVoidClicked();
    void onRefreshClicked();
    void onSearch(const QString& text);

private:
    void buildActions();
    void buildStatusTabs();
    void loadFromStorage();
    unsigned short int computeNextId() const;
    QString            suggestNextNumber() const;

    QTabBar*           m_statusTabs;
    InvoiceTableModel* m_model;
    class InvoiceFilterProxy* m_proxy;
    QList<QAction*>    m_actions;
};
