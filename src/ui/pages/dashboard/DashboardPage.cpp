#include "pages/dashboard/DashboardPage.h"
#include "components/display/KpiCard.h"
#include "components/tables/DataTableView.h"
#include "storage/StorageService.h"
#include "Invoice.h"
#include "Customer.h"
#include "Supplier.h"
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
#include <QDate>
#include <algorithm>
#include <unordered_map>

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

static QString fmtMoney(double v)
{
    return QString("$%1").arg(v, 0, 'f', 0);
}

static QString invoiceStatusName(InvoiceStatus s)
{
    switch (s) {
        case INVOICE_DRAFT:   return "Draft";
        case INVOICE_POSTED:  return "Posted";
        case INVOICE_PAID:    return "Paid";
        case INVOICE_OVERDUE: return "Overdue";
        case INVOICE_VOID:    return "Void";
        default:              return "Unknown";
    }
}

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

    m_revenueCard = new KpiCard("Revenue (YTD)", strip);
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

    vl->addWidget(makePanelHeader("Recent Invoices", card));
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

    vl->addWidget(makePanelHeader("Overdue Invoices", card));
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

    const int year = QDate::currentDate().year();
    vl->addWidget(makePanelHeader("Financial Summary", card,
                                  QString("FY %1").arg(year)));
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

void DashboardPage::onActivated()
{
    if (!StorageService::instance().isInitialized()) {
        m_receivablesCard->setValue("—");
        m_payablesCard->setValue("—");
        m_revenueCard->setValue("—");
        m_overdueCard->setValue("—");
        return;
    }

    const auto invoices  = StorageService::instance().invoices().loadAll();
    const auto customers = StorageService::instance().customers().loadAll();
    const auto suppliers = StorageService::instance().suppliers().loadAll();

    // Customer name lookup
    std::unordered_map<uint16_t, QString> custNames;
    for (const auto& c : customers)
        custNames[c.getId()] = QString::fromUtf8(c.getName());

    // KPIs
    double totalAR = 0.0;
    for (const auto& c : customers)
        if (!c.getIsDeleted()) totalAR += c.getBalance();

    double totalAP = 0.0;
    for (const auto& s : suppliers)
        if (!s.getIsDeleted()) totalAP += s.getBalance();

    const int currentYear = QDate::currentDate().year();
    const QDate today     = QDate::currentDate();
    double revenueYTD     = 0.0;
    int    overdueCount   = 0;
    double overdueTotal   = 0.0;

    for (const auto& inv : invoices) {
        if (inv.getIsDeleted()) continue;
        if (inv.getStatus() == INVOICE_PAID) {
            const QDate d = QDate::fromString(
                QString::fromUtf8(inv.getIssueDate()), "d MMM yyyy");
            if (d.year() == currentYear) revenueYTD += inv.getTotal();
        }
        if (inv.getStatus() == INVOICE_OVERDUE) {
            ++overdueCount;
            overdueTotal += inv.getTotal();
        }
    }

    m_receivablesCard->setValue(fmtMoney(totalAR));
    m_payablesCard->setValue(fmtMoney(totalAP));
    m_revenueCard->setValue(fmtMoney(revenueYTD));
    m_overdueCard->setValue(QString::number(overdueCount));
    if (overdueTotal > 0)
        m_overdueCard->setSubtitle(fmtMoney(overdueTotal) + " outstanding");

    // Recent invoices — last 6 by id
    std::vector<const Invoice*> sorted;
    sorted.reserve(invoices.size());
    for (const auto& inv : invoices)
        if (!inv.getIsDeleted()) sorted.push_back(&inv);
    std::sort(sorted.begin(), sorted.end(),
              [](const Invoice* a, const Invoice* b){ return a->getId() > b->getId(); });

    auto* rim = new QStandardItemModel(0, 6, m_recentInvoices);
    rim->setHorizontalHeaderLabels({"#","CUSTOMER","ISSUE DATE","DUE DATE","AMOUNT","STATUS"});
    for (int i = 0; i < qMin(6, (int)sorted.size()); ++i) {
        const Invoice* inv = sorted[i];
        const QString  cName = custNames.count(inv->getCustomerId())
                                   ? custNames.at(inv->getCustomerId())
                                   : QString("#%1").arg(inv->getCustomerId());
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::fromUtf8(inv->getInvoiceNumber()))
            << new QStandardItem(cName)
            << new QStandardItem(QString::fromUtf8(inv->getIssueDate()))
            << new QStandardItem(QString::fromUtf8(inv->getDueDate()))
            << new QStandardItem(QString::asprintf("$%.2f", inv->getTotal()))
            << new QStandardItem(invoiceStatusName(inv->getStatus()));
        rim->appendRow(row);
    }
    m_recentInvoices->setModel(rim);
    m_recentInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_recentInvoices->setMoneyColumns({4});
    m_recentInvoices->setStatusColumn(5);
    if (sorted.empty()) m_recentInvoices->showEmpty(true);

    // Overdue invoices — sorted by most overdue first
    std::vector<std::pair<const Invoice*, int>> overdue;  // (invoice, daysOver)
    for (const auto& inv : invoices) {
        if (inv.getIsDeleted() || inv.getStatus() != INVOICE_OVERDUE) continue;
        const QDate due = QDate::fromString(
            QString::fromUtf8(inv.getDueDate()), "d MMM yyyy");
        const int days = due.isValid() ? due.daysTo(today) : 0;
        overdue.push_back({&inv, days});
    }
    std::sort(overdue.begin(), overdue.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    auto* oim = new QStandardItemModel(0, 4, m_overdueInvoices);
    oim->setHorizontalHeaderLabels({"#","CUSTOMER","AMOUNT","DAYS OVER"});
    for (auto& [inv, days] : overdue) {
        const QString cName = custNames.count(inv->getCustomerId())
                                  ? custNames.at(inv->getCustomerId())
                                  : QString("#%1").arg(inv->getCustomerId());
        QList<QStandardItem*> row;
        row << new QStandardItem(QString::fromUtf8(inv->getInvoiceNumber()))
            << new QStandardItem(cName)
            << new QStandardItem(QString::asprintf("$%.2f", inv->getTotal()))
            << new QStandardItem(QString::number(qMax(0, days)));
        oim->appendRow(row);
    }
    m_overdueInvoices->setModel(oim);
    m_overdueInvoices->tableView()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_overdueInvoices->setMoneyColumns({2});
    m_overdueInvoices->setDaysOverColumn(3);
    if (overdue.empty()) m_overdueInvoices->showEmpty(true);

    // Financial summary
    const double net = totalAR - totalAP;
    if (m_arValue)  m_arValue->setText(fmtMoney(totalAR));
    if (m_apValue)  m_apValue->setText(fmtMoney(totalAP));
    if (m_netValue) m_netValue->setText(
        net >= 0 ? "+" + fmtMoney(net) : "-" + fmtMoney(-net));
    if (m_ytdValue) m_ytdValue->setText(fmtMoney(revenueYTD));
}
