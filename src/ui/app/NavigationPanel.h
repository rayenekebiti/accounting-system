#pragma once
#include <QWidget>
#include "common/UiTypes.h"

class QToolButton;
class QVBoxLayout;
class QPushButton;

class NavigationPanel : public QWidget {
    Q_OBJECT
public:
    explicit NavigationPanel(QWidget* parent = nullptr);

    void setActivePage(PageId id);
    void setCollapsed(bool collapsed);
    bool isCollapsed() const { return m_collapsed; }

signals:
    void navigationRequested(PageId id);

private slots:
    void onCollapseToggled();

private:
    struct NavEntry {
        PageId      id;
        QString     icon;
        QString     label;
        QToolButton* btn = nullptr;
    };

    void buildEntries();
    void applyCollapsedState();
    QToolButton* makeNavButton(NavEntry& entry);

    QVBoxLayout*    m_navLayout;
    QPushButton*    m_collapseBtn;
    bool            m_collapsed = false;

    QList<NavEntry> m_entries;
};
