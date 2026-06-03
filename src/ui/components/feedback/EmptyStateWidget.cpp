#include "components/feedback/EmptyStateWidget.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

EmptyStateWidget::EmptyStateWidget(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);

    m_iconLabel = new QLabel("📄", this);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("font-size: 48px;");

    m_titleLabel = new QLabel("No data", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet("font-size: 16px; font-weight: 600;");

    m_descLabel = new QLabel(this);
    m_descLabel->setAlignment(Qt::AlignCenter);
    m_descLabel->setObjectName("muted");
    m_descLabel->setWordWrap(true);
    m_descLabel->setMaximumWidth(320);

    m_actionBtn = new QPushButton(this);
    m_actionBtn->setVisible(false);
    m_actionBtn->setFixedWidth(160);

    layout->addWidget(m_iconLabel);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_descLabel);
    layout->addSpacing(8);
    layout->addWidget(m_actionBtn, 0, Qt::AlignCenter);

    connect(m_actionBtn, &QPushButton::clicked, this, &EmptyStateWidget::actionClicked);
}

void EmptyStateWidget::setTitle(const QString& title)       { m_titleLabel->setText(title); }
void EmptyStateWidget::setDescription(const QString& desc)  { m_descLabel->setText(desc); }

void EmptyStateWidget::setActionText(const QString& text)
{
    m_actionBtn->setText(text);
    m_actionBtn->setVisible(!text.isEmpty());
}
