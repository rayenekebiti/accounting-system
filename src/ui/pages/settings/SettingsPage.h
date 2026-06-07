#pragma once
#include "pages/base/Page.h"

class QListWidget;
class QStackedWidget;
class QPushButton;
class QLineEdit;
class QComboBox;
class QCheckBox;

class SettingsPage : public Page {
    Q_OBJECT
public:
    explicit SettingsPage(QWidget* parent = nullptr);

    PageId  pageId()    const override { return PageId::Settings; }
    QString pageTitle() const override { return "Settings"; }

private slots:
    void onCategoryChanged(int row);
    void onSaveClicked();
    void onRevertClicked();
    void markDirty();

private:
    QWidget* buildCompanyPanel();
    QWidget* buildPreferencesPanel();
    QWidget* buildNumberingPanel();
    QWidget* buildTaxPanel();
    QWidget* buildAppearancePanel();

    void loadFromStore();              // pulls every field from QSettings
    void connectDirtyTracking();        // wires every widget's change signal

    QListWidget*    m_categoryList;
    QStackedWidget* m_panelStack;
    QPushButton*    m_saveBtn;
    QPushButton*    m_revertBtn;
    bool            m_dirty = false;
    bool            m_loading = false;  // suppresses markDirty during programmatic loads

    // Company
    QLineEdit* m_companyName;
    QLineEdit* m_companyTax;
    QLineEdit* m_companyAddress;
    QLineEdit* m_companyPhone;
    QLineEdit* m_companyEmail;
    QLineEdit* m_companyWebsite;

    // Preferences
    QComboBox* m_currencyCombo;
    QComboBox* m_dateFormatCombo;
    QComboBox* m_languageCombo;

    // Numbering
    QLineEdit* m_invoicePrefix;
    QLineEdit* m_nextInvoiceNum;
    QLineEdit* m_paymentPrefix;
    QLineEdit* m_nextPaymentNum;

    // Tax
    QComboBox* m_taxSchemeCombo;
    QLineEdit* m_taxRegistrationNum;
    QCheckBox* m_pricesIncludeTax;

    // Appearance
    QComboBox* m_themeCombo;
    QComboBox* m_accentCombo;
    QComboBox* m_densityCombo;
};
