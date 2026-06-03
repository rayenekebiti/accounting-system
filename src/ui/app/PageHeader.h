#pragma once
#include <QWidget>
#include <QList>

class QLabel;
class QHBoxLayout;
class QAction;
class QToolButton;

class PageHeader : public QWidget {
    Q_OBJECT
public:
    explicit PageHeader(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setActions(const QList<QAction*>& actions);
    void clearActions();

private:
    QLabel*      m_titleLabel;
    QHBoxLayout* m_actionsLayout;

    QList<QToolButton*> m_actionButtons;
};
