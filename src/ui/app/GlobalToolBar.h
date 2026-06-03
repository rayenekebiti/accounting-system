#pragma once
#include <QToolBar>

class QLineEdit;
class QComboBox;
class QToolButton;

class GlobalToolBar : public QToolBar {
    Q_OBJECT
public:
    explicit GlobalToolBar(QWidget* parent = nullptr);

signals:
    void newInvoiceRequested();
    void newPaymentRequested();
    void newCustomerRequested();
    void globalSearchChanged(const QString& text);
    void periodChanged(const QString& period);

private:
    QToolButton* m_newBtn;
    QLineEdit*   m_searchEdit;
    QComboBox*   m_periodCombo;
};
