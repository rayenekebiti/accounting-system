#include "pages/settings/SettingsPage.h"
#include "theme/ThemeManager.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFrame>
#include <QSettings>

SettingsPage::SettingsPage(QWidget* parent) : Page(parent)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_categoryList = new QListWidget(this);
    m_categoryList->setFocusPolicy(Qt::NoFocus);
    m_categoryList->setFixedWidth(200);
    for (const QString& cat : {"Company", "Preferences", "Numbering", "Tax", "Appearance"})
        m_categoryList->addItem("  " + cat);
    m_categoryList->setCurrentRow(0);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(32, 24, 32, 16);
    rightLayout->setSpacing(0);

    m_panelStack = new QStackedWidget(rightPanel);
    m_panelStack->addWidget(buildCompanyPanel());
    m_panelStack->addWidget(buildPreferencesPanel());
    m_panelStack->addWidget(buildNumberingPanel());
    m_panelStack->addWidget(buildTaxPanel());
    m_panelStack->addWidget(buildAppearancePanel());

    auto* footer = new QWidget(rightPanel);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(0, 16, 0, 0);
    footerLayout->addStretch();

    m_revertBtn = new QPushButton("Revert", footer);
    m_revertBtn->setObjectName("secondary");
    m_revertBtn->setEnabled(false);

    m_saveBtn = new QPushButton("Save Changes", footer);
    m_saveBtn->setEnabled(false);

    footerLayout->addWidget(m_revertBtn);
    footerLayout->addWidget(m_saveBtn);

    rightLayout->addWidget(m_panelStack, 1);
    rightLayout->addWidget(footer);

    mainLayout->addWidget(m_categoryList);

    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setStyleSheet("color: #E0E0E0;");
    mainLayout->addWidget(sep);

    mainLayout->addWidget(rightPanel, 1);

    connect(m_categoryList, &QListWidget::currentRowChanged,
            this, &SettingsPage::onCategoryChanged);
    connect(m_saveBtn,   &QPushButton::clicked, this, &SettingsPage::onSaveClicked);
    connect(m_revertBtn, &QPushButton::clicked, this, &SettingsPage::onRevertClicked);

    loadFromStore();
    connectDirtyTracking();
}

static QWidget* makePanel()
{
    auto* w = new QWidget;
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    return w;
}

QWidget* SettingsPage::buildCompanyPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    m_companyName    = new QLineEdit(w);
    m_companyTax     = new QLineEdit(w);
    m_companyAddress = new QLineEdit(w);
    m_companyPhone   = new QLineEdit(w);
    m_companyEmail   = new QLineEdit(w);
    m_companyWebsite = new QLineEdit(w);

    l->addWidget(new SectionHeader("Company Profile", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Company Name", m_companyName,    w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Tax Number",   m_companyTax,     w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Address",      m_companyAddress, w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Phone",        m_companyPhone,   w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Email",        m_companyEmail,   w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Website",      m_companyWebsite, w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildPreferencesPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    m_currencyCombo = new QComboBox(w);
    m_currencyCombo->addItems({"USD – US Dollar", "EUR – Euro", "GBP – British Pound",
                               "TND – Tunisian Dinar"});

    m_dateFormatCombo = new QComboBox(w);
    m_dateFormatCombo->addItems({"DD/MM/YYYY", "MM/DD/YYYY", "YYYY-MM-DD"});

    m_languageCombo = new QComboBox(w);
    m_languageCombo->addItems({"English", "Français", "العربية"});

    l->addWidget(new SectionHeader("Display & Formatting", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Currency",    m_currencyCombo,   w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Date Format", m_dateFormatCombo, w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Language",    m_languageCombo,   w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildNumberingPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    m_invoicePrefix  = new QLineEdit("INV-", w);
    m_nextInvoiceNum = new QLineEdit("1001", w);
    m_paymentPrefix  = new QLineEdit("PAY-", w);
    m_nextPaymentNum = new QLineEdit("1001", w);

    l->addWidget(new SectionHeader("Document Numbering", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Invoice Prefix", m_invoicePrefix,  w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Next Invoice #", m_nextInvoiceNum, w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Payment Prefix", m_paymentPrefix,  w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Next Payment #", m_nextPaymentNum, w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildTaxPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    m_taxSchemeCombo = new QComboBox(w);
    m_taxSchemeCombo->addItems({"VAT (19%)", "TVA (7%)", "None"});

    m_taxRegistrationNum = new QLineEdit(w);
    m_pricesIncludeTax   = new QCheckBox("Prices include tax by default", w);

    l->addWidget(new SectionHeader("Tax Settings", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Default Tax Scheme", m_taxSchemeCombo,    w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Tax Number",         m_taxRegistrationNum, w));
    l->addSpacing(8);
    l->addWidget(m_pricesIncludeTax);
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildAppearancePanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    m_themeCombo = new QComboBox(w);
    m_themeCombo->addItems({"Dark", "Light"});
    const bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Theme::Dark;
    m_themeCombo->setCurrentIndex(isDark ? 0 : 1);
    connect(m_themeCombo, &QComboBox::currentTextChanged, this, [](const QString& t) {
        ThemeManager::instance().setTheme(
            t == "Dark" ? ThemeManager::Theme::Dark : ThemeManager::Theme::Light);
    });

    m_accentCombo = new QComboBox(w);
    m_accentCombo->addItems({"Blue (Default)", "Teal", "Purple", "Orange"});

    m_densityCombo = new QComboBox(w);
    m_densityCombo->addItems({"Comfortable (Default)", "Compact"});

    l->addWidget(new SectionHeader("Appearance", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Theme",         m_themeCombo,   w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Accent Color",  m_accentCombo,  w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Table Density", m_densityCombo, w));
    l->addStretch();
    return w;
}

void SettingsPage::loadFromStore()
{
    m_loading = true;
    QSettings s;

    m_companyName   ->setText(s.value("company/name",    "").toString());
    m_companyTax    ->setText(s.value("company/tax",     "").toString());
    m_companyAddress->setText(s.value("company/address", "").toString());
    m_companyPhone  ->setText(s.value("company/phone",   "").toString());
    m_companyEmail  ->setText(s.value("company/email",   "").toString());
    m_companyWebsite->setText(s.value("company/website", "").toString());

    m_currencyCombo  ->setCurrentIndex(s.value("prefs/currency",   0).toInt());
    m_dateFormatCombo->setCurrentIndex(s.value("prefs/dateFormat", 0).toInt());
    m_languageCombo  ->setCurrentIndex(s.value("prefs/language",   0).toInt());

    m_invoicePrefix ->setText(s.value("numbering/invoicePrefix", "INV-").toString());
    m_nextInvoiceNum->setText(s.value("numbering/nextInvoice",  "1001").toString());
    m_paymentPrefix ->setText(s.value("numbering/paymentPrefix", "PAY-").toString());
    m_nextPaymentNum->setText(s.value("numbering/nextPayment",  "1001").toString());

    m_taxSchemeCombo    ->setCurrentIndex(s.value("tax/scheme", 0).toInt());
    m_taxRegistrationNum->setText        (s.value("tax/regNumber", "").toString());
    m_pricesIncludeTax  ->setChecked     (s.value("tax/pricesInclude", false).toBool());

    m_themeCombo  ->setCurrentIndex(s.value("appearance/theme",   0).toInt());
    m_accentCombo ->setCurrentIndex(s.value("appearance/accent",  0).toInt());
    m_densityCombo->setCurrentIndex(s.value("appearance/density", 0).toInt());

    m_loading = false;
    m_dirty = false;
    m_saveBtn  ->setEnabled(false);
    m_revertBtn->setEnabled(false);
}

void SettingsPage::connectDirtyTracking()
{
    const QList<QLineEdit*> lineEdits = {
        m_companyName, m_companyTax, m_companyAddress, m_companyPhone,
        m_companyEmail, m_companyWebsite,
        m_invoicePrefix, m_nextInvoiceNum, m_paymentPrefix, m_nextPaymentNum,
        m_taxRegistrationNum
    };
    for (auto* le : lineEdits)
        connect(le, &QLineEdit::textChanged, this, &SettingsPage::markDirty);

    const QList<QComboBox*> combos = {
        m_currencyCombo, m_dateFormatCombo, m_languageCombo,
        m_taxSchemeCombo, m_themeCombo, m_accentCombo, m_densityCombo
    };
    for (auto* cb : combos)
        connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsPage::markDirty);

    connect(m_pricesIncludeTax, &QCheckBox::toggled, this, &SettingsPage::markDirty);
}

void SettingsPage::onCategoryChanged(int row)
{
    m_panelStack->setCurrentIndex(row);
}

void SettingsPage::onSaveClicked()
{
    QSettings s;
    s.setValue("company/name",    m_companyName   ->text());
    s.setValue("company/tax",     m_companyTax    ->text());
    s.setValue("company/address", m_companyAddress->text());
    s.setValue("company/phone",   m_companyPhone  ->text());
    s.setValue("company/email",   m_companyEmail  ->text());
    s.setValue("company/website", m_companyWebsite->text());

    s.setValue("prefs/currency",   m_currencyCombo  ->currentIndex());
    s.setValue("prefs/dateFormat", m_dateFormatCombo->currentIndex());
    s.setValue("prefs/language",   m_languageCombo  ->currentIndex());

    s.setValue("numbering/invoicePrefix", m_invoicePrefix ->text());
    s.setValue("numbering/nextInvoice",   m_nextInvoiceNum->text());
    s.setValue("numbering/paymentPrefix", m_paymentPrefix ->text());
    s.setValue("numbering/nextPayment",   m_nextPaymentNum->text());

    s.setValue("tax/scheme",          m_taxSchemeCombo    ->currentIndex());
    s.setValue("tax/regNumber",       m_taxRegistrationNum->text());
    s.setValue("tax/pricesInclude",   m_pricesIncludeTax  ->isChecked());

    s.setValue("appearance/theme",   m_themeCombo  ->currentIndex());
    s.setValue("appearance/accent",  m_accentCombo ->currentIndex());
    s.setValue("appearance/density", m_densityCombo->currentIndex());

    s.sync();

    m_dirty = false;
    m_saveBtn  ->setEnabled(false);
    m_revertBtn->setEnabled(false);
}

void SettingsPage::onRevertClicked()
{
    loadFromStore();
}

void SettingsPage::markDirty()
{
    if (m_loading) return;
    m_dirty = true;
    m_saveBtn  ->setEnabled(true);
    m_revertBtn->setEnabled(true);
}
