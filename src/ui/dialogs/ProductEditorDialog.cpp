#include "dialogs/ProductEditorDialog.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <stdexcept>

ProductEditorDialog::ProductEditorDialog(QWidget* parent)
    : QDialog(parent), m_id(0), m_isDeleted(false)
{
    setWindowTitle("Product");
    setModal(true);
    setMinimumWidth(520);
    buildUi();
}

void ProductEditorDialog::buildUi()
{
    m_codeEdit  = new QLineEdit(this);
    m_nameEdit  = new QLineEdit(this);
    m_descEdit  = new QLineEdit(this);

    m_priceEdit = new QDoubleSpinBox(this);
    m_priceEdit->setRange(0.0, 1.0e9);
    m_priceEdit->setDecimals(2);
    m_priceEdit->setPrefix("$ ");
    m_priceEdit->setSingleStep(1.0);

    m_costEdit  = new QDoubleSpinBox(this);
    m_costEdit->setRange(0.0, 1.0e9);
    m_costEdit->setDecimals(2);
    m_costEdit->setPrefix("$ ");
    m_costEdit->setSingleStep(1.0);

    m_stockEdit = new QSpinBox(this);
    m_stockEdit->setRange(0, 999999);

    m_codeRow  = new FormRow("SKU",         m_codeEdit,  this);
    m_nameRow  = new FormRow("Name",        m_nameEdit,  this);
    m_descRow  = new FormRow("Description", m_descEdit,  this);
    m_priceRow = new FormRow("Price",       m_priceEdit, this);
    m_costRow  = new FormRow("Cost",        m_costEdit,  this);
    m_stockRow = new FormRow("Stock",       m_stockEdit, this);

    m_nameRow->setRequired(true);

    auto* btnSave   = new QPushButton("Save",   this);
    auto* btnCancel = new QPushButton("Cancel", this);
    btnSave->setDefault(true);
    connect(btnSave,   &QPushButton::clicked, this, &ProductEditorDialog::accept);
    connect(btnCancel, &QPushButton::clicked, this, &ProductEditorDialog::reject);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(btnCancel);
    btnRow->addWidget(btnSave);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    root->addWidget(new SectionHeader("Identity", this));
    root->addWidget(m_codeRow);
    root->addWidget(m_nameRow);
    root->addWidget(m_descRow);

    root->addWidget(new SectionHeader("Pricing & Inventory", this));
    root->addWidget(m_priceRow);
    root->addWidget(m_costRow);
    root->addWidget(m_stockRow);

    root->addStretch();
    root->addLayout(btnRow);
}

void ProductEditorDialog::setForAdd(unsigned short int nextId)
{
    m_id = nextId;
    m_isDeleted = false;
    m_codeEdit->clear();
    m_nameEdit->clear();
    m_descEdit->clear();
    m_priceEdit->setValue(0.0);
    m_costEdit->setValue(0.0);
    m_stockEdit->setValue(0);
    clearErrors();
    setWindowTitle(QString("New Product (ID %1)").arg(nextId));
}

void ProductEditorDialog::setForEdit(const Product& existing)
{
    m_id = existing.getId();
    m_isDeleted = existing.getIsDeleted();
    m_codeEdit->setText(QString::fromUtf8(existing.getCode()));
    m_nameEdit->setText(QString::fromUtf8(existing.getName()));
    m_descEdit->setText(QString::fromUtf8(existing.getDescription()));
    m_priceEdit->setValue(existing.getPrice());
    m_costEdit->setValue(existing.getCost());
    m_stockEdit->setValue(existing.getStock());
    clearErrors();
    setWindowTitle(QString("Edit Product #%1").arg(m_id));
}

void ProductEditorDialog::clearErrors()
{
    m_codeRow->clearError();
    m_nameRow->clearError();
    m_descRow->clearError();
    m_priceRow->clearError();
    m_costRow->clearError();
    m_stockRow->clearError();
}

void ProductEditorDialog::accept()
{
    clearErrors();

    const QString name = m_nameEdit->text().trimmed();
    const QString code = m_codeEdit->text().trimmed();
    const QString desc = m_descEdit->text().trimmed();

    if (name.isEmpty()) {
        m_nameRow->setError("Name is required.");
        return;
    }

    try {
        const QByteArray nameBytes = name.toUtf8();
        const QByteArray codeBytes = code.toUtf8();
        const QByteArray descBytes = desc.toUtf8();

        m_result = Product(ProductData{
            m_id,
            codeBytes.constData(),
            nameBytes.constData(),
            descBytes.constData(),
            m_priceEdit->value(),
            m_costEdit->value(),
            m_stockEdit->value(),
            m_isDeleted
        });
    } catch (const std::exception& e) {
        m_nameRow->setError(QString::fromUtf8(e.what()));
        return;
    }

    QDialog::accept();
}
