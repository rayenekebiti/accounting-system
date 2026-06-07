#pragma once
#include "Payment.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class QDateEdit;
class QComboBox;
class FormRow;

class PaymentEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit PaymentEditorDialog(QWidget* parent = nullptr);

    void setForAdd(unsigned short int nextId);
    void setForEdit(const Payment& existing);

    const Payment& payment() const { return m_result; }

protected:
    void accept() override;

private slots:
    void onPartyTypeChanged(int index);

private:
    void buildUi();
    void clearErrors();
    void populatePartyCombo(int partyTypeData);

    QLineEdit*      m_numberEdit;
    QComboBox*      m_partyTypeCombo;
    QComboBox*      m_partyCombo;
    QSpinBox*       m_invoiceIdEdit;
    QDateEdit*      m_dateEdit;
    QDoubleSpinBox* m_amountEdit;
    QComboBox*      m_methodCombo;

    FormRow* m_numberRow;
    FormRow* m_partyTypeRow;
    FormRow* m_partyRow;
    FormRow* m_invoiceIdRow;
    FormRow* m_dateRow;
    FormRow* m_amountRow;
    FormRow* m_methodRow;

    unsigned short int m_id;
    bool               m_isDeleted;
    Payment            m_result;
};
