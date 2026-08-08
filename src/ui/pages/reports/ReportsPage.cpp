#include "pages/reports/ReportsPage.h"
#include "components/tables/DataTableView.h"
#include "services/Exporter.h"
#include "storage/StorageService.h"
#include "Invoice.h"
#include "Customer.h"
#include "Supplier.h"
#include "Payment.h"
#include <QLocale>
#include <QMessageBox>
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
#include <QtConcurrent/QtConcurrent>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <vector>

// ── data snapshot (captured on main thread before going off-thread) ────────────

struct Snap {
    std::vector<Customer> customers;
    std::vector<Supplier> suppliers;
    std::vector<Invoice>  invoices;
    std::vector<Payment>  payments;
    QDate                 from;
    QDate                 to;
    QString               groupBy;
};

// ── pure helpers (run in worker) ───────────────────────────────────────────────

static QString fmtM(double v)  { return QString("$%1").arg(v, 0, 'f', 2); }
static QString fmtM(Money m)   { return fmtM(m.toDouble()); }
static QString fmtN(int v)     { return QString::number(v); }
static QString fmtPct(double v){ return QString("%1%").arg(v, 0, 'f', 1); }

static QDate parseDate(const IsoDate& d)
{
    return d.isValid() ? QDate(d.year(), d.month(), d.day()) : QDate{};
}

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

// ── runner functions (all static, run in worker thread) ───────────────────────

static ReportOutput runAgedReceivables(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Customer","Current","1–30 Days","31–60 Days","61–90 Days","90+ Days","Total"};
    out.moneyCols = {1,2,3,4,5,6};
    try {
        const QDate today = QDate::currentDate();
        std::unordered_map<uint32_t,QString> custName;
        for (const auto& c : s.customers) custName[c.getId()] = QString::fromUtf8(c.getName());

        struct Bkt { double cur=0,d30=0,d60=0,d90=0,over=0; };
        std::map<uint32_t,Bkt> aging;

        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()) continue;
            const auto st = inv.getStatus();
            if (st==INVOICE_PAID||st==INVOICE_VOID||st==INVOICE_DRAFT) continue;
            const QDate due = parseDate(inv.getDueDate());
            if (!due.isValid()) continue;
            auto& b = aging[inv.getCustomerId()];
            if (due >= today) { b.cur += inv.getTotal().toDouble(); continue; }
            const int days = due.daysTo(today);
            if      (days<=30) b.d30  += inv.getTotal().toDouble();
            else if (days<=60) b.d60  += inv.getTotal().toDouble();
            else if (days<=90) b.d90  += inv.getTotal().toDouble();
            else               b.over += inv.getTotal().toDouble();
        }
        for (const auto& [id,b] : aging) {
            const double total = b.cur+b.d30+b.d60+b.d90+b.over;
            const QString nm = custName.count(id) ? custName.at(id) : QString("#%1").arg(id);
            out.rows.append({nm,fmtM(b.cur),fmtM(b.d30),fmtM(b.d60),fmtM(b.d90),fmtM(b.over),fmtM(total)});
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); }
    return out;
}

static ReportOutput runCustomerStatement(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Customer","Open Invoices","Outstanding","Paid (YTD)","Balance"};
    out.moneyCols = {2,3,4};
    try {
        std::unordered_map<uint32_t,double> outstanding,paid;
        std::unordered_map<uint32_t,int>    openCount;
        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()||inv.getStatus()==INVOICE_VOID) continue;
            if (inv.getStatus()==INVOICE_PAID)
                paid[inv.getCustomerId()] += inv.getTotal().toDouble();
            else if (inv.getStatus()!=INVOICE_DRAFT) {
                outstanding[inv.getCustomerId()] += inv.getTotal().toDouble();
                openCount[inv.getCustomerId()]++;
            }
        }
        for (const auto& c : s.customers) {
            if (c.getIsDeleted()) continue;
            const uint32_t id = c.getId();
            out.rows.append({
                QString::fromUtf8(c.getName()),
                fmtN(openCount.count(id)?openCount.at(id):0),
                fmtM(outstanding.count(id)?outstanding.at(id):0.0),
                fmtM(paid.count(id)?paid.at(id):0.0),
                fmtM(c.getBalance())
            });
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); }
    return out;
}

static ReportOutput runAgedPayables(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Supplier","Balance Owed","Email"};
    out.moneyCols = {1};
    try {
        for (const auto& sup : s.suppliers) {
            if (sup.getIsDeleted()||sup.getBalance().toDouble()<=0.0) continue;
            out.rows.append({QString::fromUtf8(sup.getName()),fmtM(sup.getBalance()),
                             QString::fromUtf8(sup.getEmail())});
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); }
    return out;
}

static ReportOutput runSupplierStatement(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Supplier","Balance","Email","Phone","Tax Number"};
    out.moneyCols = {1};
    try {
        for (const auto& sup : s.suppliers) {
            if (sup.getIsDeleted()) continue;
            out.rows.append({
                QString::fromUtf8(sup.getName()),fmtM(sup.getBalance()),
                QString::fromUtf8(sup.getEmail()),QString::fromUtf8(sup.getPhone()),
                QString::fromUtf8(sup.getTaxNumber())
            });
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); }
    return out;
}

static ReportOutput runSalesSummary(const Snap& s)
{
    ReportOutput out;
    const bool byCustomer = (s.groupBy == "Customer");
    out.headers   = {byCustomer?"Customer":"Period","Invoices","Subtotal","Tax","Total"};
    out.moneyCols = {2,3,4};

    struct Data { int count=0; double sub=0,tax=0,total=0; };
    std::map<QDate,std::pair<QString,Data>> byPeriod;
    std::map<QString,Data>                  byCust;

    try {
        std::unordered_map<uint32_t,QString> custName;
        for (const auto& c : s.customers) custName[c.getId()] = QString::fromUtf8(c.getName());

        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()) continue;
            const auto st = inv.getStatus();
            if (st==INVOICE_DRAFT||st==INVOICE_VOID) continue;
            const QDate d = parseDate(inv.getIssueDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;

            if (byCustomer) {
                const QString nm = custName.count(inv.getCustomerId())
                    ? custName.at(inv.getCustomerId()) : QString("#%1").arg(inv.getCustomerId());
                auto& data = byCust[nm];
                data.count++; data.sub+=inv.getSubtotal().toDouble();
                data.tax+=inv.getTaxAmount().toDouble(); data.total+=inv.getTotal().toDouble();
            } else {
                auto [key,label] = toPeriod(d, s.groupBy);
                auto& [lbl,data] = byPeriod[key];
                lbl = label; data.count++;
                data.sub+=inv.getSubtotal().toDouble();
                data.tax+=inv.getTaxAmount().toDouble(); data.total+=inv.getTotal().toDouble();
            }
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); return out; }

    if (byCustomer) {
        for (const auto& [nm,d] : byCust)
            out.rows.append({nm,fmtN(d.count),fmtM(d.sub),fmtM(d.tax),fmtM(d.total)});
    } else {
        for (const auto& [key,pair] : byPeriod) {
            const auto& [label,d] = pair;
            out.rows.append({label,fmtN(d.count),fmtM(d.sub),fmtM(d.tax),fmtM(d.total)});
        }
    }
    return out;
}

static ReportOutput runInvoiceRegister(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"#","Customer","Issue Date","Due Date","Subtotal","Tax","Total","Status"};
    out.moneyCols = {4,5,6};
    out.statusCol = 7;

    auto statusText = [](InvoiceStatus st) -> QString {
        switch (st) {
            case INVOICE_DRAFT:   return "Draft";
            case INVOICE_POSTED:  return "Posted";
            case INVOICE_PAID:    return "Paid";
            case INVOICE_OVERDUE: return "Overdue";
            case INVOICE_VOID:    return "Void";
            default:              return "Unknown";
        }
    };

    try {
        std::unordered_map<uint32_t,QString> custName;
        for (const auto& c : s.customers) custName[c.getId()] = QString::fromUtf8(c.getName());

        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()) continue;
            const QDate d = parseDate(inv.getIssueDate());
            if (d.isValid()&&(d<s.from||d>s.to)) continue;
            const QString cname = custName.count(inv.getCustomerId())
                ? custName.at(inv.getCustomerId()) : QString("#%1").arg(inv.getCustomerId());
            out.rows.append({
                QString::fromUtf8(inv.getInvoiceNumber()), cname,
                QString::fromStdString(inv.getIssueDate().toString()),
                QString::fromStdString(inv.getDueDate().toString()),
                fmtM(inv.getSubtotal()), fmtM(inv.getTaxAmount()), fmtM(inv.getTotal()),
                statusText(inv.getStatus())
            });
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); }
    return out;
}

static ReportOutput runTaxSummary(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Period","Invoices","Taxable Amount","Tax Amount","Total"};
    out.moneyCols = {2,3,4};
    const QString groupBy = (s.groupBy=="Customer") ? "Month" : s.groupBy;

    struct Data { int count=0; double taxable=0,tax=0,total=0; };
    std::map<QDate,std::pair<QString,Data>> byPeriod;

    try {
        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()) continue;
            const auto st = inv.getStatus();
            if (st!=INVOICE_POSTED&&st!=INVOICE_PAID) continue;
            const QDate d = parseDate(inv.getIssueDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;
            auto [key,label] = toPeriod(d,groupBy);
            auto& [lbl,data] = byPeriod[key]; lbl=label; data.count++;
            data.taxable+=inv.getSubtotal().toDouble();
            data.tax+=inv.getTaxAmount().toDouble();
            data.total+=inv.getTotal().toDouble();
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); return out; }

    for (const auto& [key,pair] : byPeriod) {
        const auto& [label,d] = pair;
        out.rows.append({label,fmtN(d.count),fmtM(d.taxable),fmtM(d.tax),fmtM(d.total)});
    }
    return out;
}

static ReportOutput runVATReturn(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Period","Output VAT","Net Sales","Gross Sales"};
    out.moneyCols = {1,2,3};
    const QString groupBy = (s.groupBy=="Customer") ? "Month" : s.groupBy;

    struct Data { double tax=0,net=0,gross=0; };
    std::map<QDate,std::pair<QString,Data>> byPeriod;

    try {
        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()) continue;
            const auto st = inv.getStatus();
            if (st!=INVOICE_POSTED&&st!=INVOICE_PAID) continue;
            const QDate d = parseDate(inv.getIssueDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;
            auto [key,label] = toPeriod(d,groupBy);
            auto& [lbl,data] = byPeriod[key]; lbl=label;
            data.tax+=inv.getTaxAmount().toDouble();
            data.net+=inv.getSubtotal().toDouble();
            data.gross+=inv.getTotal().toDouble();
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); return out; }

    for (const auto& [key,pair] : byPeriod) {
        const auto& [label,d] = pair;
        out.rows.append({label,fmtM(d.tax),fmtM(d.net),fmtM(d.gross)});
    }
    return out;
}

static ReportOutput runProfitAndLoss(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Period","Revenue","Expenses","Gross Profit","Margin %"};
    out.moneyCols = {1,2,3};
    const QString groupBy = (s.groupBy=="Customer") ? "Month" : s.groupBy;

    struct Data { double revenue=0,expenses=0; };
    std::map<QDate,std::pair<QString,Data>> byPeriod;

    try {
        for (const auto& inv : s.invoices) {
            if (inv.getIsDeleted()||inv.getStatus()!=INVOICE_PAID) continue;
            const QDate d = parseDate(inv.getIssueDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;
            auto [key,label] = toPeriod(d,groupBy);
            auto& [lbl,data] = byPeriod[key]; lbl=label;
            data.revenue += inv.getTotal().toDouble();
        }
        for (const auto& pay : s.payments) {
            if (pay.getIsDeleted()||pay.getPartyType()!=PARTY_SUPPLIER) continue;
            const QDate d = parseDate(pay.getDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;
            auto [key,label] = toPeriod(d,groupBy);
            auto& [lbl,data] = byPeriod[key]; if(lbl.isEmpty()) lbl=label;
            data.expenses += pay.getAmount().toDouble();
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); return out; }

    for (const auto& [key,pair] : byPeriod) {
        const auto& [label,d] = pair;
        const double profit = d.revenue-d.expenses;
        const double margin = d.revenue>0 ? (profit/d.revenue*100.0) : 0.0;
        out.rows.append({label,fmtM(d.revenue),fmtM(d.expenses),fmtM(profit),fmtPct(margin)});
    }
    return out;
}

static ReportOutput runCashFlow(const Snap& s)
{
    ReportOutput out;
    out.headers   = {"Period","Cash In","Cash Out","Net Cash Flow","Running Balance"};
    out.moneyCols = {1,2,3,4};
    const QString groupBy = (s.groupBy=="Customer") ? "Month" : s.groupBy;

    struct Data { double inflows=0,outflows=0; };
    std::map<QDate,std::pair<QString,Data>> byPeriod;

    try {
        for (const auto& pay : s.payments) {
            if (pay.getIsDeleted()) continue;
            const QDate d = parseDate(pay.getDate());
            if (!d.isValid()||d<s.from||d>s.to) continue;
            auto [key,label] = toPeriod(d,groupBy);
            auto& [lbl,data] = byPeriod[key]; lbl=label;
            if (pay.getPartyType()==PARTY_CUSTOMER) data.inflows  += pay.getAmount().toDouble();
            else                                    data.outflows += pay.getAmount().toDouble();
        }
    } catch (const std::exception& e) { out.error = QString::fromUtf8(e.what()); return out; }

    double running = 0.0;
    for (const auto& [key,pair] : byPeriod) {
        const auto& [label,d] = pair;
        const double net = d.inflows-d.outflows;
        running += net;
        out.rows.append({label,fmtM(d.inflows),fmtM(d.outflows),fmtM(net),fmtM(running)});
    }
    return out;
}

// ── construction ──────────────────────────────────────────────────────────────

ReportsPage::ReportsPage(QWidget* parent) : Page(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    auto* leftPanel  = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(0);

    auto* catalogLabel = new QLabel("Reports", leftPanel);
    catalogLabel->setStyleSheet("font-weight:600;font-size:13px;padding:16px 16px 8px;");
    leftLayout->addWidget(catalogLabel);

    buildCatalog();
    leftLayout->addWidget(m_catalog, 1);
    leftPanel->setFixedWidth(220);

    auto* rightPanel  = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(24,16,24,16);
    rightLayout->setSpacing(12);

    buildParameterPanel(rightPanel);

    auto* toolbar = new QHBoxLayout;
    m_runBtn = new QPushButton("Run Report", rightPanel);
    m_runBtn->setFixedWidth(120);

    m_exportBtn = new QPushButton("Export CSV", rightPanel);
    m_exportBtn->setObjectName("secondary");
    m_exportBtn->setFixedWidth(90);
    m_exportBtn->setEnabled(false);

    auto* printBtn = new QPushButton("Print", rightPanel);
    printBtn->setObjectName("secondary");
    printBtn->setFixedWidth(70);
    printBtn->setEnabled(false);
    printBtn->setToolTip("Select a report and click Print after running");

    toolbar->addWidget(m_runBtn);
    toolbar->addWidget(m_exportBtn);
    toolbar->addWidget(printBtn);
    toolbar->addStretch();
    rightLayout->addLayout(toolbar);

    m_reportLabel = new QLabel(rightPanel);
    m_reportLabel->setStyleSheet(
        "color:#6B7485;font-size:11px;font-weight:600;"
        "letter-spacing:0.5px;text-transform:uppercase;background:transparent;");
    rightLayout->addWidget(m_reportLabel);

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

    connect(m_catalog, &QListWidget::currentRowChanged, this, &ReportsPage::onReportSelected);
    connect(m_runBtn, &QPushButton::clicked, this, &ReportsPage::onRunClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, [this] {
        if (m_reportModel) Exporter::toCsv(m_reportModel, this);
    });
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
            QFont f = item->font(); f.setBold(true); item->setFont(f);
        }
    }
}

void ReportsPage::buildParameterPanel(QWidget* parent)
{
    auto* panel  = new QWidget(parent);
    auto* layout = new QHBoxLayout(panel);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(16);

    const QDate today     = QDate::currentDate();
    const QDate yearStart = QDate(today.year(), 1, 1);

    auto* fromLabel = new QLabel("From:", panel); fromLabel->setObjectName("muted");
    m_fromDate = new QDateEdit(yearStart, panel);
    m_fromDate->setDisplayFormat("dd/MM/yyyy");
    m_fromDate->setCalendarPopup(true);

    auto* toLabel = new QLabel("To:", panel); toLabel->setObjectName("muted");
    m_toDate = new QDateEdit(today, panel);
    m_toDate->setDisplayFormat("dd/MM/yyyy");
    m_toDate->setCalendarPopup(true);

    auto* groupLabel = new QLabel("Group by:", panel); groupLabel->setObjectName("muted");
    m_groupCombo = new QComboBox(panel);
    m_groupCombo->addItems({"Month","Quarter","Year","Customer"});

    layout->addWidget(fromLabel);  layout->addWidget(m_fromDate);
    layout->addWidget(toLabel);    layout->addWidget(m_toDate);
    layout->addWidget(groupLabel); layout->addWidget(m_groupCombo);
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

// ── helpers ───────────────────────────────────────────────────────────────────

QString ReportsPage::currentReportName() const
{
    const auto* item = m_catalog->currentItem();
    return item ? item->text().trimmed() : QString();
}

void ReportsPage::endReport(const ReportOutput& out)
{
    delete m_reportModel;
    m_reportModel = new QStandardItemModel(0, out.headers.size(), this);
    m_reportModel->setHorizontalHeaderLabels(out.headers);

    for (const auto& row : out.rows) {
        QList<QStandardItem*> items;
        for (int c = 0; c < row.size(); ++c) {
            auto* it = new QStandardItem(row[c]);
            if (out.moneyCols.contains(c))
                it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            items.append(it);
        }
        m_reportModel->appendRow(items);
    }

    m_resultTable->setModel(m_reportModel);
    m_resultTable->tableView()->horizontalHeader()
        ->setSectionResizeMode(QHeaderView::Stretch);
    if (!out.moneyCols.isEmpty()) m_resultTable->setMoneyColumns(out.moneyCols);
    if (out.statusCol >= 0)       m_resultTable->setStatusColumn(out.statusCol);
    m_resultTable->showBusy(false);
    m_resultTable->showEmpty(m_reportModel->rowCount() == 0);
    m_exportBtn->setEnabled(m_reportModel->rowCount() > 0);
}

// ── slots ─────────────────────────────────────────────────────────────────────

void ReportsPage::onReportSelected(int /*row*/)
{
    const QString name = currentReportName();
    if (name.isEmpty() || name.startsWith("—")) return;
    m_reportLabel->setText(name.toUpper());

    const bool grouped = (name=="Sales Summary"||name=="Tax Summary"||
                          name=="VAT Return"   ||name=="Profit & Loss"||name=="Cash Flow");
    m_groupCombo->setEnabled(grouped);
}

void ReportsPage::onRunClicked()
{
    if (!StorageService::instance().isInitialized()) {
        m_resultTable->setEmptyMessage("Storage not initialized", "No data file is open.");
        m_resultTable->showEmpty(true);
        return;
    }

    const QString name = currentReportName();
    if (name.isEmpty() || name.startsWith("—")) return;

    // Capture data snapshot on main thread (BinaryRecordFile is not thread-safe)
    Snap snap;
    snap.from    = m_fromDate->date();
    snap.to      = m_toDate->date();
    snap.groupBy = m_groupCombo->currentText();
    try {
        snap.customers = StorageService::instance().customers().loadAll();
        snap.suppliers = StorageService::instance().suppliers().loadAll();
        snap.invoices  = StorageService::instance().invoices().loadAll();
        snap.payments  = StorageService::instance().payments().loadAll();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "Load Error", QString::fromUtf8(e.what()));
        return;
    }

    m_reportLabel->setText(name.toUpper());
    m_resultTable->showBusy(true);
    m_resultTable->showEmpty(false);
    m_runBtn->setEnabled(false);
    m_exportBtn->setEnabled(false);

    // Cancel any previous task
    if (m_watcher) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
        delete m_watcher;
    }
    m_watcher = new QFutureWatcher<ReportOutput>(this);
    connect(m_watcher, &QFutureWatcher<ReportOutput>::finished,
            this, &ReportsPage::onReportFinished);

    m_watcher->setFuture(QtConcurrent::run([name, s = std::move(snap)]() -> ReportOutput {
        if      (name=="Aged Receivables")   return runAgedReceivables(s);
        else if (name=="Customer Statement") return runCustomerStatement(s);
        else if (name=="Aged Payables")      return runAgedPayables(s);
        else if (name=="Supplier Statement") return runSupplierStatement(s);
        else if (name=="Sales Summary")      return runSalesSummary(s);
        else if (name=="Invoice Register")   return runInvoiceRegister(s);
        else if (name=="Tax Summary")        return runTaxSummary(s);
        else if (name=="VAT Return")         return runVATReturn(s);
        else if (name=="Profit & Loss")      return runProfitAndLoss(s);
        else if (name=="Cash Flow")          return runCashFlow(s);
        return {};
    }));
}

void ReportsPage::onReportFinished()
{
    m_runBtn->setEnabled(true);
    const ReportOutput out = m_watcher->result();

    if (!out.error.isEmpty()) {
        m_resultTable->showBusy(false);
        m_resultTable->showEmpty(true);
        QMessageBox::critical(this, "Report Error", out.error);
        return;
    }

    endReport(out);
}
