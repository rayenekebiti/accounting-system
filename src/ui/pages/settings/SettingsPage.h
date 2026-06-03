#pragma once
#include "pages/base/Page.h"

class QListWidget;
class QStackedWidget;
class QPushButton;

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

    QListWidget*    m_categoryList;
    QStackedWidget* m_panelStack;
    QPushButton*    m_saveBtn;
    QPushButton*    m_revertBtn;
    bool            m_dirty = false;
};
