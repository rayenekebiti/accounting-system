#pragma once
#include "pages/base/ListPage.h"

class SuppliersPage : public ListPage {
    Q_OBJECT
public:
    explicit SuppliersPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Suppliers; }
    QString         pageTitle()   const override { return "Suppliers"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private:
    void buildActions();
    QList<QAction*> m_actions;
};
