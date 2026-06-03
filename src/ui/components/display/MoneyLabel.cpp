#include "components/display/MoneyLabel.h"
#include <QLocale>

MoneyLabel::MoneyLabel(QWidget* parent) : QLabel(parent)
{
    setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    setStyleSheet("font-family: 'Consolas', monospace; font-variant-numeric: tabular-nums;");
}

MoneyLabel::MoneyLabel(double amount, const QString& currency, QWidget* parent)
    : MoneyLabel(parent)
{
    m_currency = currency;
    setAmount(amount);
}

void MoneyLabel::setAmount(double amount)
{
    m_amount = amount;
    refresh();
}

void MoneyLabel::setCurrency(const QString& currency)
{
    m_currency = currency;
    refresh();
}

void MoneyLabel::setColorCoded(bool enabled)
{
    m_colorCoded = enabled;
    refresh();
}

void MoneyLabel::refresh()
{
    const QLocale locale;
    const QString formatted = locale.toCurrencyString(m_amount, m_currency);
    setText(formatted);

    if (m_colorCoded) {
        if (m_amount > 0)
            setStyleSheet("color: #2E7D32; font-family: 'Consolas', monospace;");
        else if (m_amount < 0)
            setStyleSheet("color: #C62828; font-family: 'Consolas', monospace;");
        else
            setStyleSheet("font-family: 'Consolas', monospace;");
    }
}
