#include "pages/reports/ReportsPage.h"
#include "components/tables/DataTableView.h"
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

ReportsPage::ReportsPage(QWidget* parent) : Page(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);

    // Left: report catalog
    auto* leftPanel = new QWidget(splitter);
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
    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(24, 16, 24, 16);
    rightLayout->setSpacing(16);

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

    auto* fromLabel = new QLabel("From:", panel);
    fromLabel->setObjectName("muted");
    auto* fromDate = new QDateEdit(panel);
    fromDate->setDisplayFormat("dd/MM/yyyy");
    fromDate->setCalendarPopup(true);

    auto* toLabel = new QLabel("To:", panel);
    toLabel->setObjectName("muted");
    auto* toDate = new QDateEdit(panel);
    toDate->setDisplayFormat("dd/MM/yyyy");
    toDate->setCalendarPopup(true);

    auto* groupLabel = new QLabel("Group by:", panel);
    groupLabel->setObjectName("muted");
    m_groupCombo = new QComboBox(panel);
    m_groupCombo->addItems({"Month", "Quarter", "Year", "Customer"});

    layout->addWidget(fromLabel);
    layout->addWidget(fromDate);
    layout->addWidget(toLabel);
    layout->addWidget(toDate);
    layout->addWidget(groupLabel);
    layout->addWidget(m_groupCombo);
    layout->addStretch();

    parent->layout()->addWidget(panel);
}

QFrame* ReportsPage::makeChartPlaceholder()
{
    auto* frame = new QFrame(this);
    frame->setObjectName("card");
    auto* layout = new QVBoxLayout(frame);
    auto* lbl = new QLabel("[ Chart Placeholder ]", frame);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setObjectName("muted");
    layout->addWidget(lbl, 1);
    return frame;
}

void ReportsPage::onReportSelected(int row)
{
    Q_UNUSED(row)
    // Load report parameters for the selected report
}

void ReportsPage::onRunClicked()
{
    m_resultTable->showBusy(true);
    m_resultTable->setColumns({"Period", "Amount", "Tax", "Net", "Change %"});
    m_resultTable->showBusy(false);
    m_resultTable->showEmpty(false);
}
