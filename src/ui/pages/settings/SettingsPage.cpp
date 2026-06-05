#include "pages/settings/SettingsPage.h"
#include "theme/ThemeManager.h"
#include "components/forms/FormRow.h"
#include "components/forms/SectionHeader.h"
#include <QListWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QFrame>

SettingsPage::SettingsPage(QWidget* parent) : Page(parent)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Category sidebar
    m_categoryList = new QListWidget(this);
    m_categoryList->setFocusPolicy(Qt::NoFocus);
    m_categoryList->setFixedWidth(200);
    for (const QString& cat : {"Company", "Preferences", "Numbering", "Tax", "Appearance"})
        m_categoryList->addItem("  " + cat);
    m_categoryList->setCurrentRow(0);

    // Right panel stack
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

    // Footer
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
}

static QWidget* makePanel(QWidget* parent = nullptr)
{
    auto* w = new QWidget(parent);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    w->setLayout(l);
    return w;
}

QWidget* SettingsPage::buildCompanyPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    l->addWidget(new SectionHeader("Company Profile", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Company Name", new QLineEdit(w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Tax Number",   new QLineEdit(w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Address",      new QLineEdit(w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Phone",        new QLineEdit(w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Email",        new QLineEdit(w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Website",      new QLineEdit(w), w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildPreferencesPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    l->addWidget(new SectionHeader("Display & Formatting", w));
    l->addSpacing(16);

    auto* currencyCombo = new QComboBox(w);
    currencyCombo->addItems({"USD – US Dollar", "EUR – Euro", "GBP – British Pound",
                             "TND – Tunisian Dinar"});
    l->addWidget(new FormRow("Currency", currencyCombo, w));
    l->addSpacing(8);

    auto* dateCombo = new QComboBox(w);
    dateCombo->addItems({"DD/MM/YYYY", "MM/DD/YYYY", "YYYY-MM-DD"});
    l->addWidget(new FormRow("Date Format", dateCombo, w));
    l->addSpacing(8);

    auto* langCombo = new QComboBox(w);
    langCombo->addItems({"English", "Français", "العربية"});
    l->addWidget(new FormRow("Language", langCombo, w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildNumberingPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    l->addWidget(new SectionHeader("Document Numbering", w));
    l->addSpacing(16);
    l->addWidget(new FormRow("Invoice Prefix", new QLineEdit("INV-", w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Next Invoice #", new QLineEdit("1001", w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Payment Prefix", new QLineEdit("PAY-", w), w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Next Payment #", new QLineEdit("1001", w), w));
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildTaxPanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    l->addWidget(new SectionHeader("Tax Settings", w));
    l->addSpacing(16);

    auto* taxSchemeCombo = new QComboBox(w);
    taxSchemeCombo->addItems({"VAT (19%)", "TVA (7%)", "None"});
    l->addWidget(new FormRow("Default Tax Scheme", taxSchemeCombo, w));
    l->addSpacing(8);
    l->addWidget(new FormRow("Tax Number", new QLineEdit(w), w));
    l->addSpacing(8);

    auto* incl = new QCheckBox("Prices include tax by default", w);
    l->addWidget(incl);
    l->addStretch();
    return w;
}

QWidget* SettingsPage::buildAppearancePanel()
{
    auto* w = makePanel();
    auto* l = qobject_cast<QVBoxLayout*>(w->layout());

    l->addWidget(new SectionHeader("Appearance", w));
    l->addSpacing(16);

    auto* themeCombo = new QComboBox(w);
    themeCombo->addItems({"Dark", "Light"});
    const bool isDark = ThemeManager::instance().currentTheme() == ThemeManager::Theme::Dark;
    themeCombo->setCurrentIndex(isDark ? 0 : 1);
    connect(themeCombo, &QComboBox::currentTextChanged, this, [](const QString& t) {
        ThemeManager::instance().setTheme(
            t == "Dark" ? ThemeManager::Theme::Dark : ThemeManager::Theme::Light);
    });
    l->addWidget(new FormRow("Theme", themeCombo, w));
    l->addSpacing(8);

    auto* accentCombo = new QComboBox(w);
    accentCombo->addItems({"Blue (Default)", "Teal", "Purple", "Orange"});
    l->addWidget(new FormRow("Accent Color", accentCombo, w));
    l->addSpacing(8);

    auto* density = new QComboBox(w);
    density->addItems({"Comfortable (Default)", "Compact"});
    l->addWidget(new FormRow("Table Density", density, w));
    l->addStretch();
    return w;
}

void SettingsPage::onCategoryChanged(int row)
{
    m_panelStack->setCurrentIndex(row);
}

void SettingsPage::onSaveClicked()
{
    m_dirty = false;
    m_saveBtn->setEnabled(false);
    m_revertBtn->setEnabled(false);
}

void SettingsPage::onRevertClicked()
{
    m_dirty = false;
    m_saveBtn->setEnabled(false);
    m_revertBtn->setEnabled(false);
}

void SettingsPage::markDirty()
{
    m_dirty = true;
    m_saveBtn->setEnabled(true);
    m_revertBtn->setEnabled(true);
}
