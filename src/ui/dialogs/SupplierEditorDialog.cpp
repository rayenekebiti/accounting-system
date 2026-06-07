#include "dialogs/SupplierEditorDialog.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <stdexcept>

SupplierEditorDialog::SupplierEditorDialog(QWidget* parent)
    : QDialog(parent), m_id(0), m_isDeleted(false)
{
    setWindowTitle("Supplier");
    setModal(true);
    setMinimumWidth(520);
    buildUi();
}

void SupplierEditorDialog::buildUi()
{
    m_nameEdit    = new QLineEdit(this);
    m_emailEdit   = new QLineEdit(this);
    m_phoneEdit   = new QLineEdit(this);
    m_taxEdit     = new QLineEdit(this);
    m_balanceEdit = new QDoubleSpinBox(this);
    m_balanceEdit->setRange(-1.0e9, 1.0e9);
    m_balanceEdit->setDecimals(2);
    m_balanceEdit->setPrefix("$ ");
    m_balanceEdit->setSingleStep(50.0);

    m_nameRow    = new FormRow("Name",       m_nameEdit,    this);
    m_emailRow   = new FormRow("Email",      m_emailEdit,   this);
    m_phoneRow   = new FormRow("Phone",      m_phoneEdit,   this);
    m_taxRow     = new FormRow("Tax Number", m_taxEdit,     this);
    m_balanceRow = new FormRow("Balance",    m_balanceEdit, this);

    m_nameRow->setRequired(true);

    auto* btnSave   = new QPushButton("Save",   this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnSave->setDefault(true);
    connect(btnSave,   &QPushButton::clicked, this, &SupplierEditorDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &SupplierEditorDialog::reject);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    root->addWidget(new SectionHeader("Contact", this));
    root->addWidget(m_nameRow);
    root->addWidget(m_emailRow);
    root->addWidget(m_phoneRow);

    root->addWidget(new SectionHeader("Compliance & Account", this));
    root->addWidget(m_taxRow);
    root->addWidget(m_balanceRow);

    root->addStretch();
    root->addLayout(btnRow);
}

void SupplierEditorDialog::setForAdd(unsigned short int nextId)
{
    m_id = nextId;
    m_isDeleted = false;
    m_nameEdit->clear();
    m_emailEdit->clear();
    m_phoneEdit->clear();
    m_taxEdit->clear();
    m_balanceEdit->setValue(0.0);
    clearErrors();
    setWindowTitle(QString("New Supplier (ID %1)").arg(nextId));
}

void SupplierEditorDialog::setForEdit(const Supplier& existing)
{
    m_id = existing.getId();
    m_isDeleted = existing.getIsDeleted();
    m_nameEdit->setText(QString::fromUtf8(existing.getName()));
    m_emailEdit->setText(QString::fromUtf8(existing.getEmail()));
    m_phoneEdit->setText(QString::fromUtf8(existing.getPhone()));
    m_taxEdit->setText(QString::fromUtf8(existing.getTaxNumber()));
    m_balanceEdit->setValue(existing.getBalance());
    clearErrors();
    setWindowTitle(QString("Edit Supplier #%1").arg(m_id));
}

void SupplierEditorDialog::clearErrors()
{
    m_nameRow->clearError();
    m_emailRow->clearError();
    m_phoneRow->clearError();
    m_taxRow->clearError();
    m_balanceRow->clearError();
}

void SupplierEditorDialog::accept()
{
    clearErrors();

    const QString name  = m_nameEdit->text().trimmed();
    const QString email = m_emailEdit->text().trimmed();
    const QString phone = m_phoneEdit->text().trimmed();
    const QString tax   = m_taxEdit->text().trimmed();

    bool valid = true;

    if (name.isEmpty()) {
        m_nameRow->setError("Name is required.");
        valid = false;
    }
    if (!email.isEmpty() && !email.contains('@')) {
        m_emailRow->setError("Enter a valid email address.");
        valid = false;
    }
    if (!valid) return;

    try {
        const QByteArray nameBytes  = name.toUtf8();
        const QByteArray emailBytes = email.toUtf8();
        const QByteArray phoneBytes = phone.toUtf8();
        const QByteArray taxBytes   = tax.toUtf8();

        m_result = Supplier(SupplierData{
            m_id,
            nameBytes.constData(),
            emailBytes.constData(),
            phoneBytes.constData(),
            taxBytes.constData(),
            m_balanceEdit->value(),
            m_isDeleted
        });
    } catch (const std::exception& e) {
        m_nameRow->setError(QString::fromUtf8(e.what()));
        return;
    }

    QDialog::accept();
}
