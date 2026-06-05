#pragma once
#include "pages/base/Page.h"

class KpiCard;
class DataTableView;
class QFrame;
class QWidget;
class QLabel;

class DashboardPage : public Page {
    Q_OBJECT
public:
    explicit DashboardPage(QWidget* parent = nullptr);

    PageId  pageId()    const override { return PageId::Dashboard; }
    QString pageTitle() const override { return "Dashboard"; }
    void    onActivated()    override;

private:
    QWidget* buildKpiStrip();
    QWidget* buildMainContent();
    QFrame*  buildRecentInvoicesCard();
    QWidget* buildRightPanel();
    QFrame*  buildOverdueCard();
    QFrame*  buildSummaryCard();

    KpiCard*       m_receivablesCard = nullptr;
    KpiCard*       m_payablesCard    = nullptr;
    KpiCard*       m_revenueCard     = nullptr;
    KpiCard*       m_overdueCard     = nullptr;
    DataTableView* m_recentInvoices  = nullptr;
    DataTableView* m_overdueInvoices = nullptr;

    QLabel* m_arValue  = nullptr;
    QLabel* m_apValue  = nullptr;
    QLabel* m_netValue = nullptr;
    QLabel* m_ytdValue = nullptr;
};
