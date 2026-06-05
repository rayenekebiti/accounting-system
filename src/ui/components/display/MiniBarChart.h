#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>
#include <QColor>

class MiniBarChart : public QWidget {
    Q_OBJECT
public:
    explicit MiniBarChart(QWidget* parent = nullptr);

    void setData(const QVector<double>& values, const QStringList& labels = {});
    void setBarColor(const QColor& color);
    void setPerBarColors(const QVector<QColor>& colors);
    void setShowGridlines(bool show);
    void setValuePrefix(const QString& prefix);  // e.g. "$"

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct BarGeom { int x, y, w, h; };
    QVector<BarGeom> computeGeometry(int chartH) const;
    QRect barHitRect(const BarGeom& g) const;

    QVector<double> m_values;
    QStringList     m_labels;
    QColor          m_barColor{ "#C85C38" };
    QVector<QColor> m_perBarColors;
    bool            m_showGridlines = true;
    QString         m_prefix;
    int             m_hoveredBar  = -1;
    int             m_gap         = 5;
};
