#pragma once
#include <QFrame>
#include <QColor>

class QLabel;
class QHBoxLayout;

class KpiCard : public QFrame {
    Q_OBJECT
public:
    explicit KpiCard(const QString& title, QWidget* parent = nullptr);

    void setAccentColor(const QColor& color);
    void setValue(const QString& value);
    void setDelta(double percent, bool positiveIsGood = true);
    void setSubtitle(const QString& text);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    static constexpr int kTopBarH = 2;

    QLabel*    m_titleLabel;
    QLabel*    m_valueLabel;
    QLabel*    m_arrowLabel;
    QLabel*    m_percentLabel;
    QLabel*    m_subtitleLabel;
    QColor     m_accentColor{ "#28282E" };
    bool       m_hovered = false;
};
