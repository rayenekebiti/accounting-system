#pragma once
#include "pages/base/ListPage.h"

class QTabBar;

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

private:
    void buildActions();
    void buildStatusTabs();

    QTabBar*        m_statusTabs;
    QList<QAction*> m_actions;
};
