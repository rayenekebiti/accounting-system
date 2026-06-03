#pragma once
#include <QFrame>

class QLabel;

class KpiCard : public QFrame {
    Q_OBJECT
public:
    explicit KpiCard(const QString& title, QWidget* parent = nullptr);

    void setValue(const QString& value);
    void setDelta(double percent, bool positiveIsGood = true);
    void setSubtitle(const QString& text);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    QLabel* m_titleLabel;
    QLabel* m_valueLabel;
    QLabel* m_deltaLabel;
    QLabel* m_subtitleLabel;
};
