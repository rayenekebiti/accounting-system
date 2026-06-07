#pragma once
#include "Customer.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class FormRow;

class CustomerEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomerEditorDialog(QWidget* parent = nullptr);

    // Add-mode: clears fields and sets the id that would be assigned on save.
    void setForAdd(unsigned short int nextId);

    // Edit-mode: pre-populates fields from the given customer.
    void setForEdit(const Customer& existing);

    // Returns the customer constructed when the dialog was accepted.
    // Only valid after exec() returns QDialog::Accepted.
    const Customer& customer() const { return m_result; }

protected:
    void accept() override;

private:
    void buildUi();
    void clearErrors();

    QLineEdit*      m_nameEdit;
    QLineEdit*      m_emailEdit;
    QLineEdit*      m_phoneEdit;
    QLineEdit*      m_taxEdit;
    QDoubleSpinBox* m_balanceEdit;

    FormRow* m_nameRow;
    FormRow* m_emailRow;
    FormRow* m_phoneRow;
    FormRow* m_taxRow;
    FormRow* m_balanceRow;

    unsigned short int m_id;
    bool               m_isDeleted;
    Customer           m_result;
};
