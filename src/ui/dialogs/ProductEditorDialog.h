#pragma once
#include "Product.h"
#include <QDialog>

class QLineEdit;
class QDoubleSpinBox;
class QSpinBox;
class FormRow;

class ProductEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProductEditorDialog(QWidget* parent = nullptr);

    void setForAdd(unsigned short int nextId);
    void setForEdit(const Product& existing);

    const Product& product() const { return m_result; }

protected:
    void accept() override;

private:
    void buildUi();
    void clearErrors();

    QLineEdit*      m_codeEdit;
    QLineEdit*      m_nameEdit;
    QLineEdit*      m_descEdit;
    QDoubleSpinBox* m_priceEdit;
    QDoubleSpinBox* m_costEdit;
    QSpinBox*       m_stockEdit;

    FormRow* m_codeRow;
    FormRow* m_nameRow;
    FormRow* m_descRow;
    FormRow* m_priceRow;
    FormRow* m_costRow;
    FormRow* m_stockRow;

    unsigned short int m_id;
    bool               m_isDeleted;
    Product            m_result;
};
