#pragma once
#include "pages/base/ListPage.h"

class CustomersPage : public ListPage {
    Q_OBJECT
public:
    explicit CustomersPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Customers; }
    QString         pageTitle()   const override { return "Customers"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private:
    void buildActions();
    QList<QAction*> m_actions;
};
