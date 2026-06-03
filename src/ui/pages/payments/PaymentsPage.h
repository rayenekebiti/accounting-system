#pragma once
#include "pages/base/ListPage.h"

class PaymentsPage : public ListPage {
    Q_OBJECT
public:
    explicit PaymentsPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Payments; }
    QString         pageTitle()   const override { return "Payments"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private:
    void buildActions();
    QList<QAction*> m_actions;
};
