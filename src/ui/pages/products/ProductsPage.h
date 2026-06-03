#pragma once
#include "pages/base/ListPage.h"

class ProductsPage : public ListPage {
    Q_OBJECT
public:
    explicit ProductsPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Products; }
    QString         pageTitle()   const override { return "Products"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private:
    void buildActions();
    QList<QAction*> m_actions;
};
