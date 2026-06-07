#pragma once
#include "Invoice.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class QDateEdit;
class QComboBox;
class FormRow;

class InvoiceEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit InvoiceEditorDialog(QWidget* parent = nullptr);

    void setForAdd(unsigned short int nextId,
                   const QString& suggestedNumber);
    void setForEdit(const Invoice& existing);

    const Invoice& invoice() const { return m_result; }

protected:
    void accept() override;

private slots:
    void recomputeTotal();

private:
    void buildUi();
    void clearErrors();
    void populateCustomers();

    QLineEdit*      m_numberEdit;
    QComboBox*      m_customerCombo;
    QDateEdit*      m_issueEdit;
    QDateEdit*      m_dueEdit;
    QDoubleSpinBox* m_subtotalEdit;
    QDoubleSpinBox* m_taxEdit;
    QDoubleSpinBox* m_totalEdit;
    QComboBox*      m_statusEdit;

    FormRow* m_numberRow;
    FormRow* m_customerRow;
    FormRow* m_issueRow;
    FormRow* m_dueRow;
    FormRow* m_subtotalRow;
    FormRow* m_taxRow;
    FormRow* m_totalRow;
    FormRow* m_statusRow;

    unsigned short int m_id;
    bool               m_isDeleted;
    Invoice            m_result;
};
