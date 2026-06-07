#include "dialogs/PaymentEditorDialog.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include "storage/StorageService.h"
#include "Customer.h"
#include "Supplier.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDate>
#include <stdexcept>

PaymentEditorDialog::PaymentEditorDialog(QWidget* parent)
    : QDialog(parent), m_id(0), m_isDeleted(false)
{
    setWindowTitle("Payment");
    setModal(true);
    setMinimumWidth(520);
    buildUi();
}

void PaymentEditorDialog::buildUi()
{
    m_numberEdit = new QLineEdit(this);
    m_numberEdit->setPlaceholderText("e.g. PMT-0001");

    m_partyTypeCombo = new QComboBox(this);
    m_partyTypeCombo->addItem("Customer", static_cast<int>(PARTY_CUSTOMER));
    m_partyTypeCombo->addItem("Supplier", static_cast<int>(PARTY_SUPPLIER));

    m_partyCombo = new QComboBox(this);
    m_partyCombo->setMinimumWidth(200);

    m_invoiceIdEdit = new QSpinBox(this);
    m_invoiceIdEdit->setRange(0, 65535);
    m_invoiceIdEdit->setSpecialValueText("None");

    m_dateEdit = new QDateEdit(QDate::currentDate(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat("d MMM yyyy");

    m_amountEdit = new QDoubleSpinBox(this);
    m_amountEdit->setRange(0.01, 1.0e9);
    m_amountEdit->setDecimals(2);
    m_amountEdit->setPrefix("$ ");
    m_amountEdit->setSingleStep(50.0);

    m_methodCombo = new QComboBox(this);
    m_methodCombo->addItem("Cash",  static_cast<int>(PAYMENT_CASH));
    m_methodCombo->addItem("Bank",  static_cast<int>(PAYMENT_BANK));
    m_methodCombo->addItem("Check", static_cast<int>(PAYMENT_CHECK));
    m_methodCombo->addItem("Card",  static_cast<int>(PAYMENT_CARD));

    connect(m_partyTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PaymentEditorDialog::onPartyTypeChanged);

    m_numberRow    = new FormRow("Number",     m_numberEdit,     this);
    m_partyTypeRow = new FormRow("Party Type", m_partyTypeCombo, this);
    m_partyRow     = new FormRow("Party",      m_partyCombo,     this);
    m_invoiceIdRow = new FormRow("Invoice",    m_invoiceIdEdit,  this);
    m_dateRow      = new FormRow("Date",       m_dateEdit,       this);
    m_amountRow    = new FormRow("Amount",     m_amountEdit,     this);
    m_methodRow    = new FormRow("Method",     m_methodCombo,    this);

    m_numberRow->setRequired(true);
    m_partyRow->setRequired(true);

    auto* btnSave   = new QPushButton("Save",   this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnSave->setDefault(true);
    connect(btnSave,   &QPushButton::clicked, this, &PaymentEditorDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &PaymentEditorDialog::reject);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    root->addWidget(new SectionHeader("Reference", this));
    root->addWidget(m_numberRow);
    root->addWidget(m_dateRow);
    root->addWidget(m_invoiceIdRow);

    root->addWidget(new SectionHeader("Party & Amount", this));
    root->addWidget(m_partyTypeRow);
    root->addWidget(m_partyRow);
    root->addWidget(m_amountRow);
    root->addWidget(m_methodRow);

    root->addStretch();
    root->addLayout(btnRow);

    // Populate initial party combo
    populatePartyCombo(m_partyTypeCombo->currentData().toInt());
}

void PaymentEditorDialog::populatePartyCombo(int partyTypeData)
{
    m_partyCombo->clear();
    m_partyCombo->addItem("— Select —", 0);
    if (!StorageService::instance().isInitialized()) return;

    if (partyTypeData == static_cast<int>(PARTY_CUSTOMER)) {
        for (const auto& c : StorageService::instance().customers().loadAll()) {
            if (!c.getIsDeleted())
                m_partyCombo->addItem(QString::fromUtf8(c.getName()), (int)c.getId());
        }
    } else {
        for (const auto& s : StorageService::instance().suppliers().loadAll()) {
            if (!s.getIsDeleted())
                m_partyCombo->addItem(QString::fromUtf8(s.getName()), (int)s.getId());
        }
    }
}

void PaymentEditorDialog::onPartyTypeChanged(int /*index*/)
{
    populatePartyCombo(m_partyTypeCombo->currentData().toInt());
}

void PaymentEditorDialog::setForAdd(unsigned short int nextId)
{
    m_id = nextId;
    m_isDeleted = false;
    m_numberEdit->setText(QString("PMT-%1").arg(nextId, 4, 10, QChar('0')));
    m_partyTypeCombo->setCurrentIndex(0);
    populatePartyCombo(m_partyTypeCombo->currentData().toInt());
    m_partyCombo->setCurrentIndex(0);
    m_invoiceIdEdit->setValue(0);
    m_dateEdit->setDate(QDate::currentDate());
    m_amountEdit->setValue(0.01);
    m_methodCombo->setCurrentIndex(0);
    clearErrors();
    setWindowTitle(QString("New Payment (ID %1)").arg(nextId));
}

void PaymentEditorDialog::setForEdit(const Payment& existing)
{
    m_id = existing.getId();
    m_isDeleted = existing.getIsDeleted();
    m_numberEdit->setText(QString::fromUtf8(existing.getPaymentNumber()));

    const int ptIdx = m_partyTypeCombo->findData(static_cast<int>(existing.getPartyType()));
    m_partyTypeCombo->setCurrentIndex(ptIdx >= 0 ? ptIdx : 0);

    // populatePartyCombo is triggered by the signal above; find the party by ID
    const int partyIdx = m_partyCombo->findData((int)existing.getPartyId());
    m_partyCombo->setCurrentIndex(partyIdx >= 0 ? partyIdx : 0);

    m_invoiceIdEdit->setValue(existing.getInvoiceId());

    const QDate d = QDate::fromString(
        QString::fromUtf8(existing.getDate()), "d MMM yyyy");
    m_dateEdit->setDate(d.isValid() ? d : QDate::currentDate());

    m_amountEdit->setValue(existing.getAmount());

    const int mIdx = m_methodCombo->findData(static_cast<int>(existing.getMethod()));
    m_methodCombo->setCurrentIndex(mIdx >= 0 ? mIdx : 0);

    clearErrors();
    setWindowTitle(QString("Edit Payment #%1").arg(m_id));
}

void PaymentEditorDialog::clearErrors()
{
    m_numberRow->clearError();
    m_partyTypeRow->clearError();
    m_partyRow->clearError();
    m_invoiceIdRow->clearError();
    m_dateRow->clearError();
    m_amountRow->clearError();
    m_methodRow->clearError();
}

void PaymentEditorDialog::accept()
{
    clearErrors();

    const QString number  = m_numberEdit->text().trimmed();
    const int     partyId = m_partyCombo->currentData().toInt();

    bool valid = true;
    if (number.isEmpty()) {
        m_numberRow->setError("Payment number is required.");
        valid = false;
    }
    if (partyId == 0) {
        m_partyRow->setError("Select a party.");
        valid = false;
    }
    if (!valid) return;

    try {
        const QByteArray numBytes  = number.toUtf8();
        const QByteArray dateBytes =
            m_dateEdit->date().toString("d MMM yyyy").toUtf8();

        const auto partyType = static_cast<PartyType>(
            m_partyTypeCombo->currentData().toInt());
        const auto method = static_cast<PaymentMethod>(
            m_methodCombo->currentData().toInt());

        m_result = Payment(PaymentData{
            m_id,
            numBytes.constData(),
            static_cast<unsigned short int>(m_invoiceIdEdit->value()),
            static_cast<unsigned short int>(partyId),
            partyType,
            dateBytes.constData(),
            m_amountEdit->value(),
            method,
            m_isDeleted
        });
    } catch (const std::exception& e) {
        m_numberRow->setError(QString::fromUtf8(e.what()));
        return;
    }

    QDialog::accept();
}
