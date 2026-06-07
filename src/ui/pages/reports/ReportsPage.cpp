#include "pages/reports/ReportsPage.h"
#include "components/tables/DataTableView.h"
#include "storage/StorageService.h"
#include "Invoice.h"
#include "Customer.h"
#include "Supplier.h"
#include "Payment.h"
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QDateEdit>
#include <QDate>
#include <QHeaderView>
#include <QTableView>
#include <QStandardItemModel>
#include <map>
#include <unordered_map>
#include <algorithm>

// ── helpers ───────────────────────────────────────────────────────────────────

static QString fmtM(double v)
{
    return QString("$%1").arg(v, 0, 'f', 2);
}

static QDate parseDate(const char* raw)
{
    return QDate::fromString(QString::fromUtf8(raw), "d MMM yyyy");
}

// Returns {period-start-date, display-label} for a date under the given grouping.
static std::pair<QDate, QString> toPeriod(const QDate& d, const QString& groupBy)
{
    if (groupBy == "Month")
        return {QDate(d.year(), d.month(), 1), d.toString("MMM yyyy")};
    if (groupBy == "Quarter") {
        const int q = (d.month() - 1) / 3;
        return {QDate(d.year(), q * 3 + 1, 1),
                QString("Q%1 %2").arg(q + 1).arg(d.year())};
    }
    if (groupBy == "Year")
        return {QDate(d.year(), 1, 1), QString::number(d.year())};
    return {};
}

static QStandardItem* money(double v)
{
    auto* it = new QStandardItem(fmtM(v));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

static QStandardItem* num(int v)
{
    auto* it = new QStandardItem(QString::number(v));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

static QStandardItem* pct(double v)
{
    auto* it = new QStandardItem(QString("%1%").arg(v, 0, 'f', 1));
    it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return it;
}

// ── construction ──────────────────────────────────────────────────────────────

ReportsPage::ReportsPage(QWidget* parent) : Page(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    // Left: report catalog
    auto* leftPanel  = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    auto* catalogLabel = new QLabel("Reports", leftPanel);
    catalogLabel->setStyleSheet("font-weight: 600; font-size: 13px; padding: 16px 16px 8px;");
    leftLayout->addWidget(catalogLabel);

    buildCatalog();
    leftLayout->addWidget(m_catalog, 1);
    leftPanel->setFixedWidth(220);

    // Right: parameters + results
    auto* rightPanel  = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(24, 16, 24, 16);
    rightLayout->setSpacing(12);

    buildParameterPanel(rightPanel);

    // Toolbar
    auto* toolbar = new QHBoxLayout;
    m_runBtn = new QPushButton("Run Report", rightPanel);
    m_runBtn->setFixedWidth(120);

    auto* exportBtn = new QPushButton("Export ▾", rightPanel);
    exportBtn->setObjectName("secondary");
    exportBtn->setFixedWidth(90);

    auto* printBtn = new QPushButton("Print", rightPanel);
    printBtn->setObjectName("secondary");
    printBtn->setFixedWidth(70);

    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(exportBtn);
    toolbar->addWidget(printBtn);
    toolbar->addStretch();
    rightLayout->addLayout(toolbar);

    m_reportLabel = new QLabel(rightPanel);
    m_reportLabel->setStyleSheet(
        "color: #6B7485; font-size: 11px; font-weight: 600;"
        " letter-spacing: 0.5px; text-transform: uppercase; background: transparent;");
    rightLayout->addWidget(m_reportLabel);

    // Results area
    m_results = new QStackedWidget(rightPanel);

    m_resultTable = new DataTableView(m_results);
    m_resultTable->setEmptyMessage("Select a report and press Run",
                                   "Results will appear here.");
    m_resultTable->showEmpty(true);

    auto* chartPlaceholder = makeChartPlaceholder();
    m_results->addWidget(m_resultTable);
    m_results->addWidget(chartPlaceholder);
    m_results->setCurrentIndex(0);

    rightLayout->addWidget(m_results, 1);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);

    connect(m_catalog, &QListWidget::currentRowChanged,
            this, &ReportsPage::onReportSelected);
    connect(m_runBtn, &QPushButton::clicked, this, &ReportsPage::onRunClicked);
}

void ReportsPage::buildCatalog()
{
    m_catalog = new QListWidget(this);
    m_catalog->setFocusPolicy(Qt::NoFocus);

    const QStringList reports = {
        "— Receivables —",
        "  Aged Receivables",
        "  Customer Statement",
        "— Payables —",
        "  Aged Payables",
        "  Supplier Statement",
        "— Sales —",
        "  Sales Summary",
        "  Invoice Register",
        "— Tax —",
        "  Tax Summary",
        "  VAT Return",
        "— Financials —",
        "  Profit & Loss",
        "  Cash Flow",
    };

    for (const QString& r : reports) {
        auto* item = new QListWidgetItem(r, m_catalog);
        if (r.startsWith("—")) {
            item->setFlags(Qt::NoItemFlags);
            item->setForeground(QColor("#9E9E9E"));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }
}

void ReportsPage::buildParameterPanel(QWidget* parent)
{
    auto* panel  = new QWidget(parent);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    const QDate today    = QDate::currentDate();
    const QDate yearStart = QDate(today.year(), 1, 1);

    auto* fromLabel = new QLabel("From:", panel);
    fromLabel->setObjectName("muted");
    m_fromDate = new QDateEdit(yearStart, panel);
    m_fromDate->setDisplayFormat("dd/MM/yyyy");
    m_fromDate->setCalendarPopup(true);

    auto* toLabel = new QLabel("To:", panel);
    toLabel->setObjectName("muted");
    m_toDate = new QDateEdit(today, panel);
    m_toDate->setDisplayFormat("dd/MM/yyyy");
    m_toDate->setCalendarPopup(true);

    auto* groupLabel = new QLabel("Group by:", panel);
    groupLabel->setObjectName("muted");
    m_groupCombo = new QComboBox(panel);
    m_groupCombo->addItems({"Month", "Quarter", "Year", "Customer"});

    layout->addWidget(fromLabel);
    layout->addWidget(m_fromDate);
    layout->addWidget(toLabel);
    layout->addWidget(m_toDate);
    layout->addWidget(groupLabel);
    layout->addWidget(m_groupCombo);
    layout->addStretch();

    parent->layout()->addWidget(panel);
}

QFrame* ReportsPage::makeChartPlaceholder()
{
    auto* frame  = new QFrame(this);
    frame->setObjectName("card");
    auto* layout = new QVBoxLayout(frame);
    auto* lbl    = new QLabel("[ Chart Placeholder ]", frame);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setObjectName("muted");
    layout->addWidget(lbl, 1);
    return frame;
}

// ── report helpers ────────────────────────────────────────────────────────────

QString ReportsPage::currentReportName() const
{
    const auto* item = m_catalog->currentItem();
    return item ? item->text().trimmed() : QString();
}

QStandardItemModel* ReportsPage::beginReport(const QStringList& headers)
{
    m_resultTable->showBusy(true);
    delete m_reportModel;
    m_reportModel = new QStandardItemModel(0, headers.size());
    m_reportModel->setHorizontalHeaderLabels(headers);
    return m_reportModel;
}

void ReportsPage::endReport(QStandardItemModel* model,
                             const QList<int>& moneyCols, int statusCol)
{
    m_resultTable->setModel(model);
    m_resultTable->tableView()->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);
    if (!moneyCols.isEmpty())
        m_resultTable->setMoneyColumns(moneyCols);
    if (statusCol >= 0)
        m_resultTable->setStatusColumn(statusCol);
    m_resultTable->showBusy(false);
    m_resultTable->showEmpty(model->rowCount() == 0);
}

// ── slot implementations ──────────────────────────────────────────────────────

void ReportsPage::onReportSelected(int /*row*/)
{
    const QString name = currentReportName();
    if (name.isEmpty() || name.startsWith("—")) return;
    m_reportLabel->setText(name.toUpper());

    const bool grouped = (name == "Sales Summary" || name == "Tax Summary" ||
                          name == "VAT Return"     || name == "Profit & Loss" ||
                          name == "Cash Flow");
    m_groupCombo->setEnabled(grouped);
}

void ReportsPage::onRunClicked()
{
    if (!StorageService::instance().isInitialized()) {
        m_resultTable->setEmptyMessage("Storage not initialized",
                                       "No data file is open.");
        m_resultTable->showEmpty(true);
        return;
    }

    const QString name = currentReportName();
    if      (name == "Aged Receivables")   runAgedReceivables();
    else if (name == "Customer Statement") runCustomerStatement();
    else if (name == "Aged Payables")      runAgedPayables();
    else if (name == "Supplier Statement") runSupplierStatement();
    else if (name == "Sales Summary")      runSalesSummary();
    else if (name == "Invoice Register")   runInvoiceRegister();
    else if (name == "Tax Summary")        runTaxSummary();
    else if (name == "VAT Return")         runVATReturn();
    else if (name == "Profit & Loss")      runProfitAndLoss();
    else if (name == "Cash Flow")          runCashFlow();
    else {
        m_resultTable->setEmptyMessage("Select a report",
                                       "Choose a report from the list on the left.");
        m_resultTable->showEmpty(true);
    }
}

// ── report runners ────────────────────────────────────────────────────────────

void ReportsPage::runAgedReceivables()
{
    auto* model = beginReport(
        {"Customer", "Current", "1–30 Days", "31–60 Days", "61–90 Days", "90+ Days", "Total"});

    const auto customers = StorageService::instance().customers().loadAll();
    const auto invoices  = StorageService::instance().invoices().loadAll();
    const QDate today    = QDate::currentDate();

    // customer id → name
    std::unordered_map<uint16_t, QString> custName;
    for (const auto& c : customers)
        custName[c.getId()] = QString::fromUtf8(c.getName());

    struct Buckets { double current=0, d30=0, d60=0, d90=0, dOver=0; };
    std::map<uint16_t, Buckets> aging;

    for (const auto& inv : invoices) {
        if (inv.getIsDeleted()) continue;
        const auto s = inv.getStatus();
        if (s == INVOICE_PAID || s == INVOICE_VOID || s == INVOICE_DRAFT) continue;

        const QDate due = parseDate(inv.getDueDate());
        if (!due.isValid()) continue;

        auto& b = aging[inv.getCustomerId()];
        if (due >= today) {
            b.current += inv.getTotal();
        } else {
            const int days = due.daysTo(today);
            if      (days <= 30) b.d30   += inv.getTotal();
            else if (days <= 60) b.d60   += inv.getTotal();
            else if (days <= 90) b.d90   += inv.getTotal();
            else                 b.dOver += inv.getTotal();
        }
    }

    for (const auto& [id, b] : aging) {
        const double total = b.current + b.d30 + b.d60 + b.d90 + b.dOver;
        const QString name = custName.count(id) ? custName.at(id)
                                                : QString("#%1").arg(id);
        model->appendRow({
            new QStandardItem(name),
            money(b.current), money(b.d30), money(b.d60), money(b.d90), money(b.dOver),
            money(total)
        });
    }

    endReport(model, {1, 2, 3, 4, 5, 6});
}

void ReportsPage::runCustomerStatement()
{
    auto* model = beginReport(
        {"Customer", "Open Invoices", "Outstanding", "Paid (YTD)", "Balance"});

    const auto customers = StorageService::instance().customers().loadAll();
    const auto invoices  = StorageService::instance().invoices().loadAll();

    std::unordered_map<uint16_t, double> outstanding, paid;
    std::unordered_map<uint16_t, int>    openCount;
    for (const auto& inv : invoices) {
        if (inv.getIsDeleted() || inv.getStatus() == INVOICE_VOID) continue;
        if (inv.getStatus() == INVOICE_PAID)
            paid[inv.getCustomerId()] += inv.getTotal();
        else if (inv.getStatus() != INVOICE_DRAFT) {
            outstanding[inv.getCustomerId()] += inv.getTotal();
            openCount[inv.getCustomerId()]++;
        }
    }

    for (const auto& c : customers) {
        if (c.getIsDeleted()) continue;
        const uint16_t id = c.getId();
        model->appendRow({
            new QStandardItem(QString::fromUtf8(c.getName())),
            num(openCount.count(id) ? openCount.at(id) : 0),
            money(outstanding.count(id) ? outstanding.at(id) : 0.0),
            money(paid.count(id)        ? paid.at(id)        : 0.0),
            money(c.getBalance())
        });
    }

    endReport(model, {2, 3, 4});
}

void ReportsPage::runAgedPayables()
{
    auto* model = beginReport({"Supplier", "Balance Owed", "Email"});

    for (const auto& s : StorageService::instance().suppliers().loadAll()) {
        if (s.getIsDeleted() || s.getBalance() <= 0.0) continue;
        model->appendRow({
            new QStandardItem(QString::fromUtf8(s.getName())),
            money(s.getBalance()),
            new QStandardItem(QString::fromUtf8(s.getEmail()))
        });
    }

    endReport(model, {1});
}

void ReportsPage::runSupplierStatement()
{
    auto* model = beginReport({"Supplier", "Balance", "Email", "Phone", "Tax Number"});

    for (const auto& s : StorageService::instance().suppliers().loadAll()) {
        if (s.getIsDeleted()) continue;
        model->appendRow({
            new QStandardItem(QString::fromUtf8(s.getName())),
            money(s.getBalance()),
            new QStandardItem(QString::fromUtf8(s.getEmail())),
            new QStandardItem(QString::fromUtf8(s.getPhone())),
            new QStandardItem(QString::fromUtf8(s.getTaxNumber()))
        });
    }

    endReport(model, {1});
}

void ReportsPage::runSalesSummary()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();
    const QString groupBy = m_groupCombo->currentText();

    struct Data { int count=0; double subtotal=0, tax=0, total=0; };
    const bool byCustomer = (groupBy == "Customer");

    std::map<QDate, std::pair<QString, Data>> byPeriod;
    std::map<QString, Data>                   byCust;

    const auto customers = StorageService::instance().customers().loadAll();
    std::unordered_map<uint16_t, QString> custName;
    for (const auto& c : customers)
        custName[c.getId()] = QString::fromUtf8(c.getName());

    for (const auto& inv : StorageService::instance().invoices().loadAll()) {
        if (inv.getIsDeleted()) continue;
        const auto s = inv.getStatus();
        if (s == INVOICE_DRAFT || s == INVOICE_VOID) continue;

        const QDate d = parseDate(inv.getIssueDate());
        if (!d.isValid() || d < from || d > to) continue;

        if (byCustomer) {
            const QString name = custName.count(inv.getCustomerId())
                                     ? custName.at(inv.getCustomerId())
                                     : QString("#%1").arg(inv.getCustomerId());
            auto& data = byCust[name];
            data.count++;
            data.subtotal += inv.getSubtotal();
            data.tax      += inv.getTaxAmount();
            data.total    += inv.getTotal();
        } else {
            auto [key, label] = toPeriod(d, groupBy);
            auto& [lbl, data] = byPeriod[key];
            lbl = label;
            data.count++;
            data.subtotal += inv.getSubtotal();
            data.tax      += inv.getTaxAmount();
            data.total    += inv.getTotal();
        }
    }

    auto* model = beginReport(
        {byCustomer ? "Customer" : "Period", "Invoices", "Subtotal", "Tax", "Total"});

    if (byCustomer) {
        for (const auto& [name, d] : byCust)
            model->appendRow({new QStandardItem(name), num(d.count),
                              money(d.subtotal), money(d.tax), money(d.total)});
    } else {
        for (const auto& [key, pair] : byPeriod) {
            const auto& [label, d] = pair;
            model->appendRow({new QStandardItem(label), num(d.count),
                              money(d.subtotal), money(d.tax), money(d.total)});
        }
    }

    endReport(model, {2, 3, 4});
}

void ReportsPage::runInvoiceRegister()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();

    const auto customers = StorageService::instance().customers().loadAll();
    std::unordered_map<uint16_t, QString> custName;
    for (const auto& c : customers)
        custName[c.getId()] = QString::fromUtf8(c.getName());

    auto statusText = [](InvoiceStatus s) -> QString {
        switch (s) {
            case INVOICE_DRAFT:   return "Draft";
            case INVOICE_POSTED:  return "Posted";
            case INVOICE_PAID:    return "Paid";
            case INVOICE_OVERDUE: return "Overdue";
            case INVOICE_VOID:    return "Void";
            default:              return "Unknown";
        }
    };

    auto* model = beginReport(
        {"#", "Customer", "Issue Date", "Due Date", "Subtotal", "Tax", "Total", "Status"});

    for (const auto& inv : StorageService::instance().invoices().loadAll()) {
        if (inv.getIsDeleted()) continue;
        const QDate d = parseDate(inv.getIssueDate());
        if (d.isValid() && (d < from || d > to)) continue;

        const QString cname = custName.count(inv.getCustomerId())
                                  ? custName.at(inv.getCustomerId())
                                  : QString("#%1").arg(inv.getCustomerId());
        model->appendRow({
            new QStandardItem(QString::fromUtf8(inv.getInvoiceNumber())),
            new QStandardItem(cname),
            new QStandardItem(QString::fromUtf8(inv.getIssueDate())),
            new QStandardItem(QString::fromUtf8(inv.getDueDate())),
            money(inv.getSubtotal()),
            money(inv.getTaxAmount()),
            money(inv.getTotal()),
            new QStandardItem(statusText(inv.getStatus()))
        });
    }

    endReport(model, {4, 5, 6}, 7);
}

void ReportsPage::runTaxSummary()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();
    const QString groupBy = m_groupCombo->currentText() == "Customer"
                                ? "Month" : m_groupCombo->currentText();

    struct Data { int count=0; double taxable=0, tax=0, total=0; };
    std::map<QDate, std::pair<QString, Data>> byPeriod;

    for (const auto& inv : StorageService::instance().invoices().loadAll()) {
        if (inv.getIsDeleted()) continue;
        const auto s = inv.getStatus();
        if (s != INVOICE_POSTED && s != INVOICE_PAID) continue;

        const QDate d = parseDate(inv.getIssueDate());
        if (!d.isValid() || d < from || d > to) continue;

        auto [key, label] = toPeriod(d, groupBy);
        auto& [lbl, data] = byPeriod[key];
        lbl = label;
        data.count++;
        data.taxable += inv.getSubtotal();
        data.tax     += inv.getTaxAmount();
        data.total   += inv.getTotal();
    }

    auto* model = beginReport({"Period", "Invoices", "Taxable Amount", "Tax Amount", "Total"});
    for (const auto& [key, pair] : byPeriod) {
        const auto& [label, d] = pair;
        model->appendRow({new QStandardItem(label), num(d.count),
                          money(d.taxable), money(d.tax), money(d.total)});
    }

    endReport(model, {2, 3, 4});
}

void ReportsPage::runVATReturn()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();
    const QString groupBy = m_groupCombo->currentText() == "Customer"
                                ? "Month" : m_groupCombo->currentText();

    struct Data { double tax=0, net=0, gross=0; };
    std::map<QDate, std::pair<QString, Data>> byPeriod;

    for (const auto& inv : StorageService::instance().invoices().loadAll()) {
        if (inv.getIsDeleted()) continue;
        const auto s = inv.getStatus();
        if (s != INVOICE_POSTED && s != INVOICE_PAID) continue;

        const QDate d = parseDate(inv.getIssueDate());
        if (!d.isValid() || d < from || d > to) continue;

        auto [key, label] = toPeriod(d, groupBy);
        auto& [lbl, data] = byPeriod[key];
        lbl = label;
        data.tax   += inv.getTaxAmount();
        data.net   += inv.getSubtotal();
        data.gross += inv.getTotal();
    }

    auto* model = beginReport({"Period", "Output VAT", "Net Sales", "Gross Sales"});
    for (const auto& [key, pair] : byPeriod) {
        const auto& [label, d] = pair;
        model->appendRow({new QStandardItem(label),
                          money(d.tax), money(d.net), money(d.gross)});
    }

    endReport(model, {1, 2, 3});
}

void ReportsPage::runProfitAndLoss()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();
    const QString groupBy = m_groupCombo->currentText() == "Customer"
                                ? "Month" : m_groupCombo->currentText();

    struct Data { double revenue=0, expenses=0; };
    std::map<QDate, std::pair<QString, Data>> byPeriod;

    // Revenue: paid invoices
    for (const auto& inv : StorageService::instance().invoices().loadAll()) {
        if (inv.getIsDeleted() || inv.getStatus() != INVOICE_PAID) continue;
        const QDate d = parseDate(inv.getIssueDate());
        if (!d.isValid() || d < from || d > to) continue;
        auto [key, label] = toPeriod(d, groupBy);
        auto& [lbl, data] = byPeriod[key];
        lbl = label;
        data.revenue += inv.getTotal();
    }

    // Expenses: payments to suppliers
    for (const auto& pay : StorageService::instance().payments().loadAll()) {
        if (pay.getIsDeleted() || pay.getPartyType() != PARTY_SUPPLIER) continue;
        const QDate d = parseDate(pay.getDate());
        if (!d.isValid() || d < from || d > to) continue;
        auto [key, label] = toPeriod(d, groupBy);
        auto& [lbl, data] = byPeriod[key];
        if (lbl.isEmpty()) lbl = label;
        data.expenses += pay.getAmount();
    }

    auto* model = beginReport({"Period", "Revenue", "Expenses", "Gross Profit", "Margin %"});
    for (const auto& [key, pair] : byPeriod) {
        const auto& [label, d] = pair;
        const double profit = d.revenue - d.expenses;
        const double margin = d.revenue > 0 ? (profit / d.revenue * 100.0) : 0.0;
        model->appendRow({new QStandardItem(label),
                          money(d.revenue), money(d.expenses),
                          money(profit), pct(margin)});
    }

    endReport(model, {1, 2, 3});
}

void ReportsPage::runCashFlow()
{
    const QDate from = m_fromDate->date();
    const QDate to   = m_toDate->date();
    const QString groupBy = m_groupCombo->currentText() == "Customer"
                                ? "Month" : m_groupCombo->currentText();

    struct Data { double inflows=0, outflows=0; };
    std::map<QDate, std::pair<QString, Data>> byPeriod;

    for (const auto& pay : StorageService::instance().payments().loadAll()) {
        if (pay.getIsDeleted()) continue;
        const QDate d = parseDate(pay.getDate());
        if (!d.isValid() || d < from || d > to) continue;
        auto [key, label] = toPeriod(d, groupBy);
        auto& [lbl, data] = byPeriod[key];
        lbl = label;
        if (pay.getPartyType() == PARTY_CUSTOMER)
            data.inflows  += pay.getAmount();
        else
            data.outflows += pay.getAmount();
    }

    auto* model = beginReport(
        {"Period", "Cash In", "Cash Out", "Net Cash Flow", "Running Balance"});
    double running = 0.0;
    for (const auto& [key, pair] : byPeriod) {
        const auto& [label, d] = pair;
        const double net = d.inflows - d.outflows;
        running += net;
        model->appendRow({new QStandardItem(label),
                          money(d.inflows), money(d.outflows),
                          money(net), money(running)});
    }

    endReport(model, {1, 2, 3, 4});
}
