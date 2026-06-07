#pragma once
#include "pages/base/ListPage.h"

class PaymentTableModel;
class QSortFilterProxyModel;

class PaymentsPage : public ListPage {
    Q_OBJECT
public:
    explicit PaymentsPage(QWidget* parent = nullptr);

    PageId          pageId()      const override { return PageId::Payments; }
    QString         pageTitle()   const override { return "Payments"; }
    QList<QAction*> pageActions() const override { return m_actions; }

protected:
    void onRowDoubleClicked(int row) override;

private slots:
    void onAddClicked();
    void onEditClicked();
    void onRefreshClicked();
    void onSearch(const QString& text);
    void rebuildFilter();

private:
    void buildActions();
    void loadFromStorage();
    unsigned short int computeNextId() const;

    PaymentTableModel*     m_model;
    QSortFilterProxyModel* m_proxy;
    QList<QAction*>        m_actions;
};
