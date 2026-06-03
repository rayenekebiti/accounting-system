#pragma once
#include <QLabel>

class MoneyLabel : public QLabel {
    Q_OBJECT
public:
    explicit MoneyLabel(QWidget* parent = nullptr);
    explicit MoneyLabel(double amount, const QString& currency = "USD", QWidget* parent = nullptr);

    void setAmount(double amount);
    void setCurrency(const QString& currency);
    void setColorCoded(bool enabled);

private:
    void refresh();

    double  m_amount   = 0.0;
    QString m_currency = "USD";
    bool    m_colorCoded = false;
};
