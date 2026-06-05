#include "components/display/KpiCard.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QEnterEvent>

KpiCard::KpiCard(const QString& title, QWidget* parent) : QFrame(parent)
{
    setObjectName("kpiCard");
    setFixedHeight(88);
    setMinimumWidth(160);
    setCursor(Qt::PointingHandCursor);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, kTopBarH + 10, 14, 10);
    layout->setSpacing(2);

    m_titleLabel = new QLabel(title.toUpper(), this);
    m_titleLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; letter-spacing: 0.6px;"
        " color: #6B7485; background: transparent;");

    m_valueLabel = new QLabel("—", this);
    m_valueLabel->setStyleSheet(
        "font-size: 22px; font-weight: 700; font-variant-numeric: tabular-nums;"
        " color: #C4CBD8; background: transparent; margin-top: 2px;");

    auto* trendRow    = new QWidget(this);
    auto* trendLayout = new QHBoxLayout(trendRow);
    trendLayout->setContentsMargins(0, 0, 0, 0);
    trendLayout->setSpacing(3);
    trendLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_arrowLabel   = new QLabel(this);
    m_percentLabel = new QLabel(this);
    for (auto* lbl : { m_arrowLabel, m_percentLabel })
        lbl->setStyleSheet("font-size: 11px; font-weight: 600; background: transparent;");

    trendLayout->addWidget(m_arrowLabel);
    trendLayout->addWidget(m_percentLabel);
    trendLayout->addStretch();

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setStyleSheet(
        "font-size: 11px; color: #8A93A6; background: transparent;");
    m_subtitleLabel->setVisible(false);

    layout->addWidget(m_titleLabel);
    layout->addWidget(m_valueLabel);
    layout->addWidget(trendRow);
    layout->addWidget(m_subtitleLabel);
    layout->addStretch();

    m_arrowLabel->setVisible(false);
    m_percentLabel->setVisible(false);
}

void KpiCard::setAccentColor(const QColor& color)
{
    m_accentColor = color;
    update();
}

void KpiCard::setValue(const QString& value)
{
    m_valueLabel->setText(value);
}

void KpiCard::setDelta(double percent, bool positiveIsGood)
{
    const bool positive = percent >= 0;
    const bool good     = (positive == positiveIsGood);
    const QString arrow = positive ? "▲" : "▼";
    const QString color = good ? "#268C5A" : "#C83434";
    const QString pct   = QString("%1%").arg(qAbs(percent), 0, 'f', 1);

    m_arrowLabel->setText(arrow);
    m_arrowLabel->setStyleSheet(
        QString("font-size: 10px; font-weight: 700; color: %1; background: transparent;").arg(color));
    m_percentLabel->setText(pct);
    m_percentLabel->setStyleSheet(
        QString("font-size: 11px; font-weight: 500; color: %1; background: transparent;").arg(color));

    m_arrowLabel->setVisible(true);
    m_percentLabel->setVisible(true);
}

void KpiCard::setSubtitle(const QString& text)
{
    m_subtitleLabel->setText(text);
    m_subtitleLabel->setVisible(!text.isEmpty());
}

void KpiCard::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor bg   = m_hovered ? QColor("#1D2029") : QColor("#181B22");
    const QColor bord = m_hovered ? QColor("#353D4E") : QColor("#272D3A");
    const int    r    = 3;

    // Card background
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(rect(), r, r);

    // Top accent bar (2px, clipped so corners stay rounded)
    p.setBrush(m_accentColor);
    p.setClipRect(QRect(0, 0, width(), kTopBarH));
    p.drawRoundedRect(rect(), r, r);
    p.setClipping(false);

    // Border
    p.setPen(QPen(bord, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);

    QFrame::paintEvent(event);
}

void KpiCard::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QFrame::enterEvent(event);
}

void KpiCard::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QFrame::leaveEvent(event);
}

void KpiCard::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QFrame::mousePressEvent(event);
}
