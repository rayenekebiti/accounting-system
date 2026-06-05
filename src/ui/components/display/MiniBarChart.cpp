#include "components/display/MiniBarChart.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QToolTip>
#include <algorithm>
#include <cmath>

MiniBarChart::MiniBarChart(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(100);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void MiniBarChart::setData(const QVector<double>& values, const QStringList& labels)
{
    m_values = values;
    m_labels = labels;
    m_hoveredBar = -1;
    update();
}

void MiniBarChart::setBarColor(const QColor& color)
{
    m_barColor = color;
    update();
}

void MiniBarChart::setPerBarColors(const QVector<QColor>& colors)
{
    m_perBarColors = colors;
    update();
}

void MiniBarChart::setShowGridlines(bool show)
{
    m_showGridlines = show;
    update();
}

void MiniBarChart::setValuePrefix(const QString& prefix)
{
    m_prefix = prefix;
    update();
}

QVector<MiniBarChart::BarGeom> MiniBarChart::computeGeometry(int chartH) const
{
    const int n    = m_values.size();
    const int barW = n > 0 ? (width() - m_gap * (n - 1)) / n : 0;
    const double maxVal = m_values.isEmpty() ? 1.0
        : *std::max_element(m_values.begin(), m_values.end());

    QVector<BarGeom> geoms;
    geoms.reserve(n);
    for (int i = 0; i < n; ++i) {
        const int barH = std::max(3, int((m_values[i] / maxVal) * chartH));
        geoms.push_back({ i * (barW + m_gap), chartH - barH, barW, barH });
    }
    return geoms;
}

QRect MiniBarChart::barHitRect(const BarGeom& g) const
{
    return { g.x, 0, g.w, height() };
}

void MiniBarChart::paintEvent(QPaintEvent*)
{
    if (m_values.isEmpty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int labelH  = m_labels.isEmpty() ? 0 : 16;
    const int topPad  = 20;  // room for value label above tallest bar
    const int chartH  = height() - labelH - topPad;
    const int originY = topPad + chartH;

    // ── Gridlines (3 horizontal at 25/50/75% of max) ─────────────────────
    if (m_showGridlines) {
        p.setPen(QPen(QColor(30, 30, 36), 1, Qt::SolidLine));
        for (int pct : {25, 50, 75}) {
            const int lineY = topPad + chartH - int(pct / 100.0 * chartH);
            p.drawLine(0, lineY, width(), lineY);
        }
    }

    // ── Baseline ─────────────────────────────────────────────────────────
    p.setPen(QPen(QColor(40, 40, 46), 1));
    p.drawLine(0, originY, width(), originY);

    const auto geoms  = computeGeometry(chartH);
    const int  n      = geoms.size();
    const double maxVal = *std::max_element(m_values.begin(), m_values.end());

    // find tallest bar for value label
    int tallestIdx = 0;
    for (int i = 1; i < n; ++i)
        if (m_values[i] > m_values[tallestIdx]) tallestIdx = i;

    for (int i = 0; i < n; ++i) {
        const BarGeom& g = geoms[i];
        const bool hovered = (i == m_hoveredBar);

        // Bar color
        QColor base;
        if (!m_perBarColors.isEmpty() && i < m_perBarColors.size())
            base = m_perBarColors[i];
        else
            base = m_barColor;

        if (hovered) {
            // Brighten on hover
            base = base.lighter(130);
        }

        QLinearGradient grad(0, topPad + g.y, 0, topPad + g.y + g.h);
        grad.setColorAt(0.0, base);
        QColor faded = base;
        faded.setAlphaF(0.35);
        grad.setColorAt(1.0, faded);

        QPainterPath path;
        path.addRoundedRect(QRectF(g.x, topPad + g.y, g.w, g.h), 3, 3);
        p.fillPath(path, grad);

        // Value label above the tallest bar only
        if (i == tallestIdx) {
            const QString valStr = m_prefix + QString::number(int(m_values[i]));
            p.setPen(QColor(240, 237, 232, 200));
            p.setFont(QFont("Segoe UI", 8, QFont::DemiBold));
            const QRect lblR(g.x - 4, topPad + g.y - 18, g.w + 8, 16);
            p.drawText(lblR, Qt::AlignHCenter | Qt::AlignBottom, valStr);
        }

        // X-axis label
        if (i < m_labels.size()) {
            p.setPen(QColor(138, 134, 128, 170));
            p.setFont(QFont("Segoe UI", 8));
            p.drawText(QRect(g.x, originY + 2, g.w, labelH),
                       Qt::AlignHCenter | Qt::AlignTop, m_labels[i]);
        }
    }
}

void MiniBarChart::mouseMoveEvent(QMouseEvent* event)
{
    if (m_values.isEmpty()) return;

    const int labelH = m_labels.isEmpty() ? 0 : 16;
    const int chartH = height() - labelH - 20;
    const auto geoms = computeGeometry(chartH);

    int hit = -1;
    for (int i = 0; i < geoms.size(); ++i) {
        if (barHitRect(geoms[i]).contains(event->pos())) {
            hit = i;
            break;
        }
    }

    if (hit != m_hoveredBar) {
        m_hoveredBar = hit;
        update();
    }

    if (hit >= 0 && hit < m_values.size()) {
        const QString label  = (hit < m_labels.size()) ? m_labels[hit] : QString::number(hit + 1);
        const QString valStr = m_prefix + QString("%1").arg(m_values[hit], 0, 'f', 0);
        QToolTip::showText(event->globalPosition().toPoint(),
                           QString("<b>%1</b><br>%2").arg(label, valStr), this);
    } else {
        QToolTip::hideText();
    }

    QWidget::mouseMoveEvent(event);
}

void MiniBarChart::leaveEvent(QEvent* event)
{
    m_hoveredBar = -1;
    QToolTip::hideText();
    update();
    QWidget::leaveEvent(event);
}
