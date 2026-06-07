#include "dialogs/InvoiceEditorDialog.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include "storage/StorageService.h"
#include "Customer.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDate>
#include <stdexcept>

InvoiceEditorDialog::InvoiceEditorDialog(QWidget* parent)
    : QDialog(parent), m_id(0), m_isDeleted(false)
{
    setWindowTitle("Invoice");
    setModal(true);
    setMinimumWidth(580);
    buildUi();
}

void InvoiceEditorDialog::buildUi()
{
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

    m_subtotalEdit = new QDoubleSpinBox(this);
    m_subtotalEdit->setRange(0.0, 1.0e9);
    m_subtotalEdit->setDecimals(2);
    m_subtotalEdit->setPrefix("$ ");

    m_taxEdit = new QDoubleSpinBox(this);
    m_taxEdit->setRange(0.0, 1.0e9);
    m_taxEdit->setDecimals(2);
    m_taxEdit->setPrefix("$ ");

    m_totalEdit = new QDoubleSpinBox(this);
    m_totalEdit->setRange(0.0, 1.0e9);
    m_totalEdit->setDecimals(2);
    m_totalEdit->setPrefix("$ ");
    m_totalEdit->setReadOnly(true);
    m_totalEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);

    m_statusEdit = new QComboBox(this);
    m_statusEdit->addItem("Draft",   INVOICE_DRAFT);
    m_statusEdit->addItem("Posted",  INVOICE_POSTED);
    m_statusEdit->addItem("Paid",    INVOICE_PAID);
    m_statusEdit->addItem("Overdue", INVOICE_OVERDUE);
    m_statusEdit->addItem("Void",    INVOICE_VOID);

    connect(m_subtotalEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InvoiceEditorDialog::recomputeTotal);
    connect(m_taxEdit, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &InvoiceEditorDialog::recomputeTotal);

    m_numberRow   = new FormRow("Number",    m_numberEdit,    this);
    m_customerRow = new FormRow("Customer",  m_customerCombo, this);
    m_issueRow    = new FormRow("Issue Date", m_issueEdit,    this);
    m_dueRow      = new FormRow("Due Date",  m_dueEdit,       this);
    m_subtotalRow = new FormRow("Subtotal",  m_subtotalEdit,  this);
    m_taxRow      = new FormRow("Tax",       m_taxEdit,       this);
    m_totalRow    = new FormRow("Total",     m_totalEdit,     this);
    m_statusRow   = new FormRow("Status",    m_statusEdit,    this);

    m_numberRow->setRequired(true);
    m_customerRow->setRequired(true);

    auto* btnSave   = new QPushButton("Save",   this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnSave->setDefault(true);
    connect(btnSave,   &QPushButton::clicked, this, &InvoiceEditorDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &InvoiceEditorDialog::reject);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    root->addWidget(new SectionHeader("Header", this));
    root->addWidget(m_numberRow);
    root->addWidget(m_customerRow);
    root->addWidget(m_issueRow);
    root->addWidget(m_dueRow);

    root->addWidget(new SectionHeader("Amounts", this));
    root->addWidget(m_subtotalRow);
    root->addWidget(m_taxRow);
    root->addWidget(m_totalRow);

    root->addWidget(new SectionHeader("Status", this));
    root->addWidget(m_statusRow);

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
            m_customerCombo->addItem(QString::fromUtf8(c.getName()), (int)c.getId());
    }
}

void InvoiceEditorDialog::recomputeTotal()
{
    m_totalEdit->setValue(m_subtotalEdit->value() + m_taxEdit->value());
}

void InvoiceEditorDialog::setForAdd(unsigned short int nextId,
                                     const QString& suggestedNumber)
{
    m_id = nextId;
    m_isDeleted = false;
    populateCustomers();
    m_numberEdit->setText(suggestedNumber);
    m_customerCombo->setCurrentIndex(0);
    m_issueEdit->setDate(QDate::currentDate());
    m_dueEdit->setDate(QDate::currentDate().addDays(30));
    m_subtotalEdit->setValue(0.0);
    m_taxEdit->setValue(0.0);
    m_totalEdit->setValue(0.0);
    m_statusEdit->setCurrentIndex(0);
    clearErrors();
    setWindowTitle("New Invoice");
}

void InvoiceEditorDialog::setForEdit(const Invoice& existing)
{
    m_id = existing.getId();
    m_isDeleted = existing.getIsDeleted();
    populateCustomers();
    m_numberEdit->setText(QString::fromUtf8(existing.getInvoiceNumber()));

    const int custIdx = m_customerCombo->findData((int)existing.getCustomerId());
    m_customerCombo->setCurrentIndex(custIdx >= 0 ? custIdx : 0);

    const QDate issue = QDate::fromString(
        QString::fromUtf8(existing.getIssueDate()), "d MMM yyyy");
    m_issueEdit->setDate(issue.isValid() ? issue : QDate::currentDate());

    const QDate due = QDate::fromString(
        QString::fromUtf8(existing.getDueDate()), "d MMM yyyy");
    m_dueEdit->setDate(due.isValid() ? due : QDate::currentDate().addDays(30));

    m_subtotalEdit->setValue(existing.getSubtotal());
    m_taxEdit->setValue(existing.getTaxAmount());
    m_totalEdit->setValue(existing.getTotal());

    const int statusIdx = m_statusEdit->findData(existing.getStatus());
    m_statusEdit->setCurrentIndex(statusIdx >= 0 ? statusIdx : 0);

    clearErrors();
    setWindowTitle(QString("Edit Invoice %1").arg(
        QString::fromUtf8(existing.getInvoiceNumber())));
}

void InvoiceEditorDialog::clearErrors()
{
    m_numberRow->clearError();
    m_customerRow->clearError();
    m_issueRow->clearError();
    m_dueRow->clearError();
    m_subtotalRow->clearError();
    m_taxRow->clearError();
    m_totalRow->clearError();
    m_statusRow->clearError();
}

void InvoiceEditorDialog::accept()
{
    clearErrors();

    const QString number = m_numberEdit->text().trimmed();
    const int customerId = m_customerCombo->currentData().toInt();

    bool valid = true;
    if (number.isEmpty()) {
        m_numberRow->setError("Invoice number is required.");
        valid = false;
    }
    if (customerId == 0) {
        m_customerRow->setError("Select a customer.");
        valid = false;
    }
    if (m_dueEdit->date() < m_issueEdit->date()) {
        m_dueRow->setError("Due date can't be before issue date.");
        valid = false;
    }
    if (!valid) return;

    try {
        const QByteArray numberBytes = number.toUtf8();
        const QByteArray issueBytes  =
            m_issueEdit->date().toString("d MMM yyyy").toUtf8();
        const QByteArray dueBytes    =
            m_dueEdit->date().toString("d MMM yyyy").toUtf8();

        const InvoiceStatus status =
            static_cast<InvoiceStatus>(m_statusEdit->currentData().toInt());

        m_result = Invoice(InvoiceData{
            m_id,
            numberBytes.constData(),
            static_cast<unsigned short int>(customerId),
            issueBytes.constData(),
            dueBytes.constData(),
            m_subtotalEdit->value(),
            m_taxEdit->value(),
            m_totalEdit->value(),
            status,
            m_isDeleted
        });
    } catch (const std::exception& e) {
        m_numberRow->setError(QString::fromUtf8(e.what()));
        return;
    }

    QDialog::accept();
}
