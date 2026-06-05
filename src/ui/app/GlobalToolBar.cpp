#include "app/GlobalToolBar.h"
#include <QLineEdit>
#include <QComboBox>
#include <QToolButton>
#include <QMenu>
#include <QAction>
#include <QLabel>
#include <QWidget>
#include <QHBoxLayout>

GlobalToolBar::GlobalToolBar(QWidget* parent) : QToolBar("Global Toolbar", parent)
{
    setMovable(false);
    setFixedHeight(44);
    setIconSize(QSize(14, 14));

    // "New" primary action — enterprise blue
    m_newBtn = new QToolButton(this);
    m_newBtn->setText("+ New");
    m_newBtn->setStyleSheet(
        "QToolButton { font-weight: 600; padding: 5px 14px; background: #1A6FE0;"
        " color: white; border-radius: 3px; border: none; font-size: 13px; }"
        "QToolButton:hover { background: #155DC0; }"
        "QToolButton::menu-indicator { image: none; }");
    m_newBtn->setPopupMode(QToolButton::InstantPopup);

    auto* newMenu    = new QMenu(m_newBtn);
    auto* newInvoice  = newMenu->addAction("Invoice");
    auto* newPayment  = newMenu->addAction("Payment");
    newMenu->addSeparator();
    auto* newCustomer = newMenu->addAction("Customer");
    auto* newSupplier = newMenu->addAction("Supplier");
    auto* newProduct  = newMenu->addAction("Product");
    m_newBtn->setMenu(newMenu);

    connect(newInvoice,  &QAction::triggered, this, &GlobalToolBar::newInvoiceRequested);
    connect(newPayment,  &QAction::triggered, this, &GlobalToolBar::newPaymentRequested);
    connect(newCustomer, &QAction::triggered, this, &GlobalToolBar::newCustomerRequested);
    Q_UNUSED(newSupplier) Q_UNUSED(newProduct)

    addWidget(m_newBtn);
    addSeparator();

    // Global search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search invoices, customers, payments…");
    m_searchEdit->setMinimumWidth(280);
    m_searchEdit->setMaximumWidth(380);
    addWidget(m_searchEdit);

    // Spacer
    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    // Fiscal period
    auto* periodLabel = new QLabel("Period:", this);
    periodLabel->setObjectName("muted");
    addWidget(periodLabel);

    m_periodCombo = new QComboBox(this);
    m_periodCombo->addItems({"FY 2026", "FY 2025", "Q2 2026", "Q1 2026",
                             "This Month", "Last Month", "Custom…"});
    m_periodCombo->setFixedWidth(110);
    addWidget(m_periodCombo);

    addSeparator();

    auto* settingsBtn = new QToolButton(this);
    settingsBtn->setText("⚙");
    addWidget(settingsBtn);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &GlobalToolBar::globalSearchChanged);
    connect(m_periodCombo, &QComboBox::currentTextChanged,
            this, &GlobalToolBar::periodChanged);
}
