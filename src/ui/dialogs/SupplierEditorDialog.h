#pragma once
#include "Supplier.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class FormRow;

class SupplierEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit SupplierEditorDialog(QWidget* parent = nullptr);

    void setForAdd(unsigned short int nextId);
    void setForEdit(const Supplier& existing);

    const Supplier& supplier() const { return m_result; }

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
    Supplier           m_result;
};
