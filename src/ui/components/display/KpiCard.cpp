#include "components/display/KpiCard.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QMouseEvent>

KpiCard::KpiCard(const QString& title, QWidget* parent) : QFrame(parent)
{
    setObjectName("card");
    setFixedHeight(110);
    setMinimumWidth(180);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setObjectName("muted");
    m_titleLabel->setStyleSheet("font-size: 12px;");

    m_valueLabel = new QLabel("—", this);
    m_valueLabel->setStyleSheet("font-size: 22px; font-weight: 700;");

    m_deltaLabel = new QLabel(this);
    m_deltaLabel->setStyleSheet("font-size: 12px;");
    m_deltaLabel->setVisible(false);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName("muted");
    m_subtitleLabel->setStyleSheet("font-size: 11px;");
    m_subtitleLabel->setVisible(false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_valueLabel);
    layout->addWidget(m_deltaLabel);
    layout->addWidget(m_subtitleLabel);
    layout->addStretch();
}

void KpiCard::setValue(const QString& value)
{
    m_valueLabel->setText(value);
}

void KpiCard::setDelta(double percent, bool positiveIsGood)
{
    const bool positive = percent >= 0;
    const QString arrow = positive ? "▲" : "▼";
    const QString color = (positive == positiveIsGood) ? "#2E7D32" : "#C62828";
    m_deltaLabel->setText(
        QString("<span style='color:%1'>%2 %3%</span>").arg(color, arrow).arg(qAbs(percent), 0, 'f', 1));
    m_deltaLabel->setVisible(true);
}

void KpiCard::setSubtitle(const QString& text)
{
    m_subtitleLabel->setText(text);
    m_subtitleLabel->setVisible(!text.isEmpty());
}

void KpiCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}
