#pragma once
#include "pages/base/Page.h"

class KpiCard;
class DataTableView;
class QFrame;
class QScrollArea;

class DashboardPage : public Page {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);

    PageId  pageId()    const override { return PageId::Dashboard; }
    QString pageTitle() const override { return "Dashboard"; }
    void    onActivated()    override;

private:
    void buildKpiRow(QWidget* container);
    void buildChartsRow(QWidget* container);
    void buildListsRow(QWidget* container);

    QFrame* makeChartCard(const QString& title);

    KpiCard* m_receivablesCard;
    KpiCard* m_payablesCard;
    KpiCard* m_revenueCard;
    KpiCard* m_overdueCard;

    DataTableView* m_recentInvoices;
    DataTableView* m_overdueInvoices;
};
