#include "pages/dashboard/DashboardPage.h"
#include "components/display/KpiCard.h"
#include "components/tables/DataTableView.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableView>
#include <QStandardItemModel>

// ── Helpers ──────────────────────────────────────────────────────────────────

static QFrame* makeSeparator(QWidget* parent = nullptr)
{
    auto* sep = new QFrame(parent);
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet("background: #272D3A; border: none;");
    return sep;
}

static QWidget* makePanelHeader(const QString& title, QWidget* parent = nullptr,
                                const QString& trailingMeta = QString())
{
    auto* bar = new QWidget(parent);
    bar->setFixedHeight(34);
    auto* hl = new QHBoxLayout(bar);
    hl->setContentsMargins(14, 0, 14, 0);

    auto* lbl = new QLabel(title.toUpper(), bar);
    lbl->setObjectName("sectionTitle");
    hl->addWidget(lbl);
    hl->addStretch();

    if (!trailingMeta.isEmpty()) {
        auto* meta = new QLabel(trailingMeta, bar);
        meta->setStyleSheet(
            "color: #6B7485; font-size: 10px; font-weight: 600;"
            " letter-spacing: 0.4px; background: transparent;");
        hl->addWidget(meta);
    }

    return bar;
}

// ── DashboardPage ─────────────────────────────────────────────────────────────

DashboardPage::DashboardPage(QWidget* parent) : Page(parent)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* vl      = new QVBoxLayout(content);
    vl->setContentsMargins(20, 20, 20, 20);
    vl->setSpacing(16);

    vl->addWidget(buildKpiStrip());
    vl->addWidget(makeSeparator());
    vl->addWidget(buildMainContent(), 1);
    vl->addStretch();

    scroll->setWidget(content);

    auto* ml = new QVBoxLayout(this);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->addWidget(scroll);
}

QWidget* DashboardPage::buildKpiStrip()
{
    auto* strip  = new QWidget;
    auto* layout = new QHBoxLayout(strip);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    m_receivablesCard = new KpiCard("Total Receivables", strip);
    m_receivablesCard->setAccentColor(QColor("#268C5A"));

    m_payablesCard = new KpiCard("Total Payables", strip);
    m_payablesCard->setAccentColor(QColor("#BA7B2A"));

    m_revenueCard = new KpiCard("Revenue (Period)", strip);
    m_revenueCard->setAccentColor(QColor("#1A6FE0"));

    m_overdueCard = new KpiCard("Overdue Invoices", strip);
    m_overdueCard->setAccentColor(QColor("#C83434"));

    layout->addWidget(m_receivablesCard, 1);
    layout->addWidget(m_payablesCard,    1);
    layout->addWidget(m_revenueCard,     1);
    layout->addWidget(m_overdueCard,     1);
    return strip;
}

QWidget* DashboardPage::buildMainContent()
{
    auto* row    = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    layout->addWidget(buildRecentInvoicesCard(), 58);
    layout->addWidget(buildRightPanel(),         42);
    return row;
}

QFrame* DashboardPage::buildRecentInvoicesCard()
{
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(makePanelHeader("Recent Invoices", card, "6 records"));
    vl->addWidget(makeSeparator(card));

    m_recentInvoices = new DataTableView(card);
    m_recentInvoices->setColumns({"#", "CUSTOMER", "ISSUE DATE", "DUE DATE", "AMOUNT", "STATUS"});
    m_recentInvoices->tableView()->setSortingEnabled(false);
    m_recentInvoices->tableView()->horizontalHeader()->setSortIndicatorShown(false);
    m_recentInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_recentInvoices->setCompactMode(true);
    vl->addWidget(m_recentInvoices, 1);

    return card;
}

QWidget* DashboardPage::buildRightPanel()
{
    auto* panel = new QWidget;
    auto* vl    = new QVBoxLayout(panel);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(14);

    vl->addWidget(buildOverdueCard(),  3);
    vl->addWidget(buildSummaryCard(),  2);
    return panel;
}

QFrame* DashboardPage::buildOverdueCard()
{
    auto* card = new QFrame;
    card->setObjectName("dangerCard");
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(makePanelHeader("Overdue Invoices", card, "4 records"));
    vl->addWidget(makeSeparator(card));

    m_overdueInvoices = new DataTableView(card);
    m_overdueInvoices->setColumns({"#", "CUSTOMER", "AMOUNT", "DAYS OVER"});
    m_overdueInvoices->tableView()->setSortingEnabled(false);
    m_overdueInvoices->tableView()->horizontalHeader()->setSortIndicatorShown(false);
    m_overdueInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_overdueInvoices->setCompactMode(true);
    vl->addWidget(m_overdueInvoices, 1);

    return card;
}

QFrame* DashboardPage::buildSummaryCard()
{
    auto* card = new QFrame;
    card->setObjectName("card");
    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    vl->addWidget(makePanelHeader("Financial Summary", card, "FY 2026"));
    vl->addWidget(makeSeparator(card));

    auto* body = new QWidget(card);
    auto* grid = new QGridLayout(body);
    grid->setContentsMargins(14, 10, 14, 10);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(8);

    auto addRow = [&](int row, const QString& label, QLabel*& valOut) {
        auto* lbl = new QLabel(label, body);
        lbl->setStyleSheet("font-size: 11px; color: #6B7485; background: transparent;");

        valOut = new QLabel("—", body);
        valOut->setStyleSheet(
            "font-size: 12px; font-weight: 600; color: #C4CBD8; background: transparent;");
        valOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        grid->addWidget(lbl,   row, 0, Qt::AlignLeft | Qt::AlignVCenter);
        grid->addWidget(valOut,row, 1, Qt::AlignRight | Qt::AlignVCenter);
    };

    addRow(0, "Total Receivables",  m_arValue);
    addRow(1, "Total Payables",     m_apValue);
    addRow(2, "Net Cash Position",  m_netValue);
    addRow(3, "Revenue YTD",        m_ytdValue);

    grid->setColumnStretch(0, 1);
    vl->addWidget(body, 1);
    return card;
}

// ── Data population ───────────────────────────────────────────────────────────

void DashboardPage::onActivated()
{
    // KPI cards
    m_receivablesCard->setValue("$24,850");
    m_receivablesCard->setDelta(12.5, true);

    m_payablesCard->setValue("$8,320");
    m_payablesCard->setDelta(-3.2, false);

    m_revenueCard->setValue("$61,400");
    m_revenueCard->setDelta(8.1, true);

    m_overdueCard->setValue("7");
    m_overdueCard->setSubtitle("$4,230 outstanding");

    // Recent invoices — 6 columns
    auto* rim = new QStandardItemModel(0, 6, m_recentInvoices);
    rim->setHorizontalHeaderLabels({"#", "CUSTOMER", "ISSUE DATE", "DUE DATE", "AMOUNT", "STATUS"});
    auto addInv = [&](const QStringList& r) {
        QList<QStandardItem*> items;
        for (const QString& c : r) items << new QStandardItem(c);
        rim->appendRow(items);
    };
    addInv({"INV-1024", "Acme Corp",    "01 Jun 2026", "30 Jun 2026", "$4,200.00", "Paid"   });
    addInv({"INV-1023", "Beta LLC",     "28 May 2026", "27 Jun 2026", "$1,850.00", "Posted" });
    addInv({"INV-1022", "Gamma Inc",    "12 May 2026", "11 Jun 2026", "$7,400.00", "Overdue"});
    addInv({"INV-1021", "Delta & Co",   "20 May 2026", "19 Jun 2026", "$920.00",   "Paid"   });
    addInv({"INV-1020", "Epsilon Ltd",  "15 May 2026", "14 Jun 2026", "$3,150.00", "Draft"  });
    addInv({"INV-1019", "Zeta Corp",    "10 May 2026", "09 Jun 2026", "$2,800.00", "Posted" });
    m_recentInvoices->setModel(rim);
    m_recentInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_recentInvoices->setMoneyColumns({4});
    m_recentInvoices->setStatusColumn(5);

    // Overdue invoices
    auto* oim = new QStandardItemModel(0, 4, m_overdueInvoices);
    oim->setHorizontalHeaderLabels({"#", "CUSTOMER", "AMOUNT", "DAYS OVER"});
    auto addOvr = [&](const QStringList& r) {
        QList<QStandardItem*> items;
        for (const QString& c : r) items << new QStandardItem(c);
        oim->appendRow(items);
    };
    addOvr({"INV-1022", "Gamma Inc",    "$7,400.00", "18"});
    addOvr({"INV-1018", "Zeta Corp",    "$2,100.00", "32"});
    addOvr({"INV-1015", "Eta Partners", "$1,980.00", "47"});
    addOvr({"INV-1009", "Theta SA",     "$3,620.00", "61"});
    m_overdueInvoices->setModel(oim);
    m_overdueInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_overdueInvoices->setMoneyColumns({2});
    m_overdueInvoices->setDaysOverColumn(3);

    // Financial summary
    if (m_arValue)  m_arValue->setText("$24,850");
    if (m_apValue)  m_apValue->setText("$8,320");
    if (m_netValue) m_netValue->setText("+$16,530");
    if (m_ytdValue) m_ytdValue->setText("$61,400");
}
