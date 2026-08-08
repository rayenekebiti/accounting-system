#include "dialogs/InvoiceEditorDialog.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include "storage/StorageService.h"
#include "Customer.h"
#include "Product.h"
#include <QLineEdit>
#include <QLabel>
#include <QDateEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDate>
#include <stdexcept>
#include <cmath>

InvoiceEditorDialog::InvoiceEditorDialog(QWidget* parent)
    : QDialog(parent), m_id(0), m_isDeleted(false)
{
    setWindowTitle("Invoice");
    setModal(true);
    setMinimumWidth(740);
    buildUi();
}

void InvoiceEditorDialog::buildUi()
{
    // --- Header fields ---
    m_numberEdit = new QLineEdit(this);

    m_customerCombo = new QComboBox(this);
    m_customerCombo->setMinimumWidth(200);
    populateCustomers();

    m_issueEdit = new QDateEdit(QDate::currentDate(), this);
    m_issueEdit->setCalendarPopup(true);
    m_issueEdit->setDisplayFormat("d MMM yyyy");

    m_dueEdit = new QDateEdit(QDate::currentDate().addDays(30), this);
    m_dueEdit->setCalendarPopup(true);
    m_dueEdit->setDisplayFormat("d MMM yyyy");

    m_numberRow   = new FormRow("Number",     m_numberEdit,    this);
    m_customerRow = new FormRow("Customer",   m_customerCombo, this);
    m_issueRow    = new FormRow("Issue Date", m_issueEdit,     this);
    m_dueRow      = new FormRow("Due Date",   m_dueEdit,       this);
    m_numberRow->setRequired(true);
    m_customerRow->setRequired(true);

    // --- Product picker bar ---
    m_productCombo = new QComboBox(this);
    m_productCombo->setMinimumWidth(200);
    populateProducts();

    m_addProductBtn = new QPushButton("Add Product", this);
    m_addProductBtn->setFixedWidth(110);

    auto* pickerBar = new QWidget(this);
    auto* pickerLayout = new QHBoxLayout(pickerBar);
    pickerLayout->setContentsMargins(0, 0, 0, 0);
    pickerLayout->setSpacing(6);
    auto* productLbl = new QLabel("Product:", pickerBar);
    productLbl->setStyleSheet("font-size: 11px; color: #6B7485; background: transparent;");
    pickerLayout->addWidget(productLbl);
    pickerLayout->addWidget(m_productCombo, 1);
    pickerLayout->addWidget(m_addProductBtn);

    // --- Line action bar ---
    m_addBlankBtn = new QPushButton("Add Blank Line", this);
    m_removeBtn   = new QPushButton("Remove Line",    this);
    m_removeBtn->setEnabled(false);

    auto* lineBtnBar = new QWidget(this);
    auto* lineBtnLayout = new QHBoxLayout(lineBtnBar);
    lineBtnLayout->setContentsMargins(0, 0, 0, 0);
    lineBtnLayout->setSpacing(6);
    lineBtnLayout->addWidget(m_addBlankBtn);
    lineBtnLayout->addStretch();
    lineBtnLayout->addWidget(m_removeBtn);

    // --- Line items table ---
    m_linesTable = new QTableWidget(0, 5, this);
    m_linesTable->setHorizontalHeaderLabels({"Description", "Qty", "Unit Price", "Tax %", "Total"});
    m_linesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_linesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_linesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_linesTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_linesTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_linesTable->setColumnWidth(1, 64);
    m_linesTable->setColumnWidth(2, 96);
    m_linesTable->setColumnWidth(3, 72);
    m_linesTable->setColumnWidth(4, 96);
    m_linesTable->setMinimumHeight(190);
    m_linesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_linesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_linesTable->verticalHeader()->setVisible(false);
    m_linesTable->setAlternatingRowColors(true);

    connect(m_linesTable, &QTableWidget::cellChanged,
            this, &InvoiceEditorDialog::onCellChanged);
    connect(m_linesTable, &QTableWidget::itemSelectionChanged, this, [this] {
        m_removeBtn->setEnabled(m_linesTable->currentRow() >= 0);
    });

    // --- Totals section ---
    auto* totalsWidget = new QWidget(this);
    auto* totalsGrid = new QGridLayout(totalsWidget);
    totalsGrid->setContentsMargins(0, 4, 0, 4);
    totalsGrid->setHorizontalSpacing(16);
    totalsGrid->setVerticalSpacing(4);

    auto mkLbl = [&](const QString& text) -> QLabel* {
        auto* l = new QLabel(text, totalsWidget);
        l->setStyleSheet("font-size: 11px; color: #6B7485; background: transparent;");
        return l;
    };
    auto mkVal = [&](QLabel*& ptr) -> QLabel* {
        ptr = new QLabel("$0.00", totalsWidget);
        ptr->setStyleSheet(
            "font-size: 12px; font-weight: 600; color: #C4CBD8; background: transparent;");
        ptr->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        return ptr;
    };

    totalsGrid->addWidget(mkLbl("Subtotal"), 0, 0, Qt::AlignLeft);
    totalsGrid->addWidget(mkVal(m_subtotalLabel), 0, 1, Qt::AlignRight);
    totalsGrid->addWidget(mkLbl("Tax"),      1, 0, Qt::AlignLeft);
    totalsGrid->addWidget(mkVal(m_taxLabel), 1, 1, Qt::AlignRight);
    totalsGrid->addWidget(mkLbl("Total"),    2, 0, Qt::AlignLeft);
    totalsGrid->addWidget(mkVal(m_totalLabel), 2, 1, Qt::AlignRight);
    totalsGrid->setColumnStretch(0, 1);

    // --- Status ---
    m_statusEdit = new QComboBox(this);
    m_statusEdit->addItem("Draft",   INVOICE_DRAFT);
    m_statusEdit->addItem("Posted",  INVOICE_POSTED);
    m_statusEdit->addItem("Paid",    INVOICE_PAID);
    m_statusEdit->addItem("Overdue", INVOICE_OVERDUE);
    m_statusEdit->addItem("Void",    INVOICE_VOID);
    auto* statusRow = new FormRow("Status", m_statusEdit, this);

    // --- Buttons ---
    auto* btnSave   = new QPushButton("Save",   this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnSave->setDefault(true);
    connect(btnSave,   &QPushButton::clicked, this, &InvoiceEditorDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &InvoiceEditorDialog::reject);
    connect(m_addProductBtn, &QPushButton::clicked, this, &InvoiceEditorDialog::onAddProductLine);
    connect(m_addBlankBtn,   &QPushButton::clicked, this, &InvoiceEditorDialog::onAddBlankLine);
    connect(m_removeBtn,     &QPushButton::clicked, this, &InvoiceEditorDialog::onRemoveLine);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    // --- Root layout ---
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(10);

    root->addWidget(new SectionHeader("Header", this));
    root->addWidget(m_numberRow);
    root->addWidget(m_customerRow);
    root->addWidget(m_issueRow);
    root->addWidget(m_dueRow);

    root->addWidget(new SectionHeader("Line Items", this));
    root->addWidget(pickerBar);
    root->addWidget(lineBtnBar);
    root->addWidget(m_linesTable);

    root->addWidget(new SectionHeader("Totals", this));
    root->addWidget(totalsWidget);

    root->addWidget(new SectionHeader("Status", this));
    root->addWidget(statusRow);

    root->addStretch();
    root->addLayout(btnRow);
}

void InvoiceEditorDialog::populateCustomers()
{
    m_customerCombo->clear();
    m_customerCombo->addItem("— Select customer —", 0);
    if (!StorageService::instance().isInitialized()) return;
    for (const auto& c : StorageService::instance().customers().loadAll()) {
        if (!c.getIsDeleted())
            m_customerCombo->addItem(QString::fromUtf8(c.getName()),
                                     static_cast<int>(c.getId()));
    }
}

void InvoiceEditorDialog::populateProducts()
{
    m_productCombo->clear();
    m_productCombo->addItem("— Select product —", 0);
    if (!StorageService::instance().isInitialized()) return;
    for (const auto& p : StorageService::instance().products().loadAll()) {
        if (!p.getIsDeleted()) {
            const QString label = QString("%1  [$%2]")
                .arg(QString::fromUtf8(p.getName()))
                .arg(p.getPrice().toDouble(), 0, 'f', 2);
            m_productCombo->addItem(label, static_cast<int>(p.getId()));
        }
    }
}

void InvoiceEditorDialog::addTableRow(const QString& desc,
                                       double qty, double price,
                                       double taxPct, double total,
                                       uint32_t productId, uint32_t lineId)
{
    m_updatingTable = true;
    const int row = m_linesTable->rowCount();
    m_linesTable->insertRow(row);

    auto* descItem = new QTableWidgetItem(desc);
    m_linesTable->setItem(row, 0, descItem);

    auto* qtyItem = new QTableWidgetItem(QString::number(qty, 'f', 3));
    qtyItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_linesTable->setItem(row, 1, qtyItem);

    auto* priceItem = new QTableWidgetItem(QString::number(price, 'f', 2));
    priceItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_linesTable->setItem(row, 2, priceItem);

    auto* taxItem = new QTableWidgetItem(QString::number(taxPct, 'f', 2));
    taxItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_linesTable->setItem(row, 3, taxItem);

    auto* totalItem = new QTableWidgetItem(QString::number(total, 'f', 2));
    totalItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_linesTable->setItem(row, 4, totalItem);

    m_lineIds.append(lineId);
    m_lineProductIds.append(productId);
    m_updatingTable = false;

    recomputeTotals();
}

void InvoiceEditorDialog::onAddProductLine()
{
    const int idx = m_productCombo->currentIndex();
    if (idx <= 0) { addTableRow(); return; }

    const uint32_t productId = static_cast<uint32_t>(
        m_productCombo->itemData(idx).toInt());

    if (!StorageService::instance().isInitialized()) {
        addTableRow({}, 1.0, 0.0, 0.0, 0.0, productId, 0);
        return;
    }
    for (const auto& p : StorageService::instance().products().loadAll()) {
        if (p.getId() != productId || p.getIsDeleted()) continue;
        const double price = p.getPrice().toDouble();
        addTableRow(QString::fromUtf8(p.getName()),
                    1.0, price, 0.0, price, productId, 0);
        return;
    }
    addTableRow({}, 1.0, 0.0, 0.0, 0.0, productId, 0);
}

void InvoiceEditorDialog::onAddBlankLine()
{
    addTableRow();
}

void InvoiceEditorDialog::onRemoveLine()
{
    const int row = m_linesTable->currentRow();
    if (row < 0) return;
    m_updatingTable = true;
    m_linesTable->removeRow(row);
    if (row < m_lineIds.size())       m_lineIds.removeAt(row);
    if (row < m_lineProductIds.size()) m_lineProductIds.removeAt(row);
    m_updatingTable = false;
    recomputeTotals();
    m_removeBtn->setEnabled(m_linesTable->currentRow() >= 0);
}

void InvoiceEditorDialog::onCellChanged(int row, int col)
{
    if (m_updatingTable) return;
    if (col < 1 || col > 3) return;

    auto cellDouble = [&](int c) -> double {
        auto* item = m_linesTable->item(row, c);
        return item ? item->text().toDouble() : 0.0;
    };
    const double qty    = cellDouble(1);
    const double price  = cellDouble(2);
    const double taxPct = cellDouble(3);
    const double sub    = qty * price;
    const double total  = sub + sub * (taxPct / 100.0);

    m_updatingTable = true;
    auto* t = m_linesTable->item(row, 4);
    if (t) t->setText(QString::number(total, 'f', 2));
    m_updatingTable = false;

    recomputeTotals();
}

void InvoiceEditorDialog::recomputeTotals()
{
    double subtotal = 0.0, tax = 0.0;
    for (int r = 0; r < m_linesTable->rowCount(); ++r) {
        auto cellD = [&](int c) {
            auto* it = m_linesTable->item(r, c);
            return it ? it->text().toDouble() : 0.0;
        };
        const double lineSub = cellD(1) * cellD(2);
        subtotal += lineSub;
        tax      += lineSub * (cellD(3) / 100.0);
    }
    m_subtotalLabel->setText(QString("$%1").arg(subtotal, 0, 'f', 2));
    m_taxLabel->setText(QString("$%1").arg(tax, 0, 'f', 2));
    m_totalLabel->setText(QString("$%1").arg(subtotal + tax, 0, 'f', 2));
}

void InvoiceEditorDialog::clearErrors()
{
    m_numberRow->clearError();
    m_customerRow->clearError();
    m_issueRow->clearError();
    m_dueRow->clearError();
}

void InvoiceEditorDialog::setForAdd(uint32_t nextId,
                                     const QString& suggestedNumber)
{
    m_id = nextId;
    m_isDeleted = false;
    populateCustomers();
    populateProducts();
    m_numberEdit->setText(suggestedNumber);
    m_customerCombo->setCurrentIndex(0);
    m_issueEdit->setDate(QDate::currentDate());
    m_dueEdit->setDate(QDate::currentDate().addDays(30));
    m_statusEdit->setCurrentIndex(0);
    m_updatingTable = true;
    m_linesTable->setRowCount(0);
    m_lineIds.clear();
    m_lineProductIds.clear();
    m_updatingTable = false;
    recomputeTotals();
    clearErrors();
    setWindowTitle("New Invoice");
}

void InvoiceEditorDialog::setForEdit(const Invoice& existing)
{
    m_id = existing.getId();
    m_isDeleted = existing.getIsDeleted();
    populateCustomers();
    populateProducts();

    m_numberEdit->setText(QString::fromUtf8(existing.getInvoiceNumber()));

    const int custIdx = m_customerCombo->findData(static_cast<int>(existing.getCustomerId()));
    m_customerCombo->setCurrentIndex(custIdx >= 0 ? custIdx : 0);

    auto isoToQDate = [](IsoDate d, QDate fallback) {
        return d.isValid() ? QDate(d.year(), d.month(), d.day()) : fallback;
    };
    m_issueEdit->setDate(isoToQDate(existing.getIssueDate(), QDate::currentDate()));
    m_dueEdit->setDate(isoToQDate(existing.getDueDate(), QDate::currentDate().addDays(30)));

    const int statusIdx = m_statusEdit->findData(existing.getStatus());
    m_statusEdit->setCurrentIndex(statusIdx >= 0 ? statusIdx : 0);

    // Load existing lines from storage
    m_updatingTable = true;
    m_linesTable->setRowCount(0);
    m_lineIds.clear();
    m_lineProductIds.clear();
    m_updatingTable = false;

    if (StorageService::instance().isInitialized()) {
        for (const auto& line :
             StorageService::instance().invoiceLines().findByInvoice(m_id)) {
            addTableRow(QString::fromUtf8(line.getDescription()),
                        line.getQuantity(),
                        line.getUnitPrice().toDouble(),
                        line.getTaxRatePermille() / 10.0,
                        line.getLineTotal().toDouble(),
                        line.getProductId(),
                        line.getId());
        }
    }

    recomputeTotals();
    clearErrors();
    setWindowTitle(QString("Edit Invoice %1")
                   .arg(QString::fromUtf8(existing.getInvoiceNumber())));
}

void InvoiceEditorDialog::accept()
{
    clearErrors();

    const QString number  = m_numberEdit->text().trimmed();
    const int     custIdx = m_customerCombo->currentIndex();
    const int customerId  = (custIdx > 0) ? m_customerCombo->itemData(custIdx).toInt() : 0;

    bool valid = true;
    if (number.isEmpty()) {
        m_numberRow->setError("Invoice number is required.");
        valid = false;
    }
    if (custIdx <= 0) {
        m_customerRow->setError("Select a customer.");
        valid = false;
    }
    if (m_dueEdit->date() < m_issueEdit->date()) {
        m_dueRow->setError("Due date can't be before issue date.");
        valid = false;
    }
    if (!valid) return;

    // Collect line items from table and compute totals
    double subtotal = 0.0, tax = 0.0;
    m_resultLines.clear();
    for (int r = 0; r < m_linesTable->rowCount(); ++r) {
        auto cellD = [&](int c) {
            auto* it = m_linesTable->item(r, c);
            return it ? it->text().toDouble() : 0.0;
        };
        const QString desc   = m_linesTable->item(r, 0)
                               ? m_linesTable->item(r, 0)->text() : "";
        const double  qty    = cellD(1);
        const double  price  = cellD(2);
        const double  taxPct = cellD(3);
        const double  lineSub = qty * price;
        subtotal += lineSub;
        tax      += lineSub * (taxPct / 100.0);

        const QByteArray descBytes = desc.toUtf8();
        InvoiceLineData ld;
        ld.id                 = (r < m_lineIds.size()) ? m_lineIds[r] : 0;
        ld.invoiceId          = m_id;
        ld.productId          = (r < m_lineProductIds.size()) ? m_lineProductIds[r] : 0;
        ld.description        = descBytes.constData();
        ld.quantityMilliunits = static_cast<int32_t>(std::round(qty * 1000.0));
        ld.unitPrice          = Money::fromDouble(price);
        ld.taxRatePermille    = static_cast<int16_t>(std::round(taxPct * 10.0));
        ld.lineTotal          = Money::fromDouble(lineSub + lineSub * (taxPct / 100.0));
        ld.isDeleted          = false;
        m_resultLines.emplace_back(ld);
    }

    try {
        const QByteArray numberBytes = number.toUtf8();
        auto toIso = [](QDate d) {
            auto opt = IsoDate::fromString(d.toString("yyyy-MM-dd").toStdString());
            return opt.value_or(IsoDate{});
        };
        const InvoiceStatus status =
            static_cast<InvoiceStatus>(m_statusEdit->currentData().toInt());

        m_result = Invoice(InvoiceData{
            m_id,
            numberBytes.constData(),
            static_cast<uint32_t>(customerId),
            toIso(m_issueEdit->date()),
            toIso(m_dueEdit->date()),
            Money::fromDouble(subtotal),
            Money::fromDouble(tax),
            Money::fromDouble(subtotal + tax),
            status,
            m_isDeleted
        });
    } catch (const std::exception& e) {
        m_numberRow->setError(QString::fromUtf8(e.what()));
        return;
    }

    QDialog::accept();
}
