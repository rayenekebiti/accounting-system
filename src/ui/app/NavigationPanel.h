#pragma once
#include <QWidget>
#include "common/UiTypes.h"

class QListWidget;
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
    void onItemClicked(int row);
    void onCollapseToggled();

private:
    struct NavEntry {
        PageId  id;
        QString icon;
        QString label;
    };

    void buildEntries();
    void applyCollapsedState();

    QListWidget* m_list;
    QPushButton* m_collapseBtn;
    bool         m_collapsed = false;

    QList<NavEntry> m_entries;
};
