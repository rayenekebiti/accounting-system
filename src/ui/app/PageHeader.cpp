#include "app/PageHeader.h"
#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QAction>

PageHeader::PageHeader(QWidget* parent) : QWidget(parent)
{
    setObjectName("pageHeaderWidget");
    setFixedHeight(46);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 16, 0);
    layout->setSpacing(16);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("pageTitle");

    layout->addWidget(m_titleLabel);
    layout->addStretch();

    m_actionsLayout = new QHBoxLayout;
    m_actionsLayout->setSpacing(6);
    layout->addLayout(m_actionsLayout);
}

void PageHeader::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void PageHeader::setActions(const QList<QAction*>& actions)
{
    clearActions();
    for (QAction* action : actions) {
        auto* btn = new QToolButton(this);
        btn->setDefaultAction(action);
        btn->setStyleSheet(
            "QToolButton { background: transparent; border: 1px solid #272D3A;"
            " color: #C4CBD8; border-radius: 3px; padding: 4px 12px;"
            " font-weight: 500; font-size: 13px; }"
            "QToolButton:hover { background: rgba(26,111,224,0.10); border-color: #1A6FE0;"
            " color: #1A6FE0; }");
        m_actionsLayout->addWidget(btn);
        m_actionButtons.append(btn);
    }
}

void PageHeader::clearActions()
{
    for (auto* btn : m_actionButtons) {
        m_actionsLayout->removeWidget(btn);
        btn->deleteLater();
    }
    m_actionButtons.clear();
}
