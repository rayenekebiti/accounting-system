#pragma once
#include "pages/base/ListPage.h"

class CustomerTableModel;
class QSortFilterProxyModel;

class CustomersPage : public ListPage {
    Q_OBJECT
public:
    explicit CustomersPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Customers; }
    QString         pageTitle()   const override { return "Customers"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private slots:
    void onAddClicked();
    void onEditClicked();
    void onDeactivateClicked();
    void onRefreshClicked();
    void onSearch(const QString& text);
    void rebuildFilter();

private:
    void buildActions();
    void loadFromStorage();
    unsigned short int computeNextId() const;

    CustomerTableModel*    m_model;
    QSortFilterProxyModel* m_proxy;
    QList<QAction*>        m_actions;
};
