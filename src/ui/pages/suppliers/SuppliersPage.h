#pragma once
#include "pages/base/ListPage.h"

class SupplierTableModel;
class QSortFilterProxyModel;

class SuppliersPage : public ListPage {
    Q_OBJECT
public:
    explicit SuppliersPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Suppliers; }
    QString         pageTitle()   const override { return "Suppliers"; }
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

    SupplierTableModel*    m_model;
    QSortFilterProxyModel* m_proxy;
    QList<QAction*>        m_actions;
};
