#pragma once
#include "Invoice.h"
#include "InvoiceLine.h"
#include <QDialog>
#include <QList>
#include <vector>

class QLineEdit;
class QLabel;
class QDateEdit;
class QComboBox;
class QTableWidget;
class QPushButton;
class FormRow;

class InvoiceEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit InvoiceEditorDialog(QWidget* parent = nullptr);

    void setForAdd(uint32_t nextId, const QString& suggestedNumber);
    void setForEdit(const Invoice& existing);

    const Invoice& invoice() const { return m_result; }
    const std::vector<InvoiceLine>& lines() const { return m_resultLines; }

protected:
    void accept() override;

private slots:
    void onAddProductLine();
    void onAddBlankLine();
    void onRemoveLine();
    void onCellChanged(int row, int col);

private:
    void buildUi();
    void clearErrors();
    void populateCustomers();
    void populateProducts();
    void addTableRow(const QString& desc = {},
                     double qty = 1.0,
                     double price = 0.0,
                     double taxPct = 0.0,
                     double total = 0.0,
                     uint32_t productId = 0,
                     uint32_t lineId = 0);
    void recomputeTotals();

    // Header
    QLineEdit*   m_numberEdit;
    QComboBox*   m_customerCombo;
    QDateEdit*   m_issueEdit;
    QDateEdit*   m_dueEdit;

    // Line items
    QComboBox*    m_productCombo;
    QTableWidget* m_linesTable;
    QPushButton*  m_addProductBtn;
    QPushButton*  m_addBlankBtn;
    QPushButton*  m_removeBtn;

    // Totals (read-only labels)
    QLabel* m_subtotalLabel;
    QLabel* m_taxLabel;
    QLabel* m_totalLabel;

    // Status
    QComboBox* m_statusEdit;

    // Form rows (for error display)
    FormRow* m_numberRow;
    FormRow* m_customerRow;
    FormRow* m_issueRow;
    FormRow* m_dueRow;

    // Parallel arrays — one entry per table row
    QList<uint32_t> m_lineIds;       // existing record IDs (0 = new)
    QList<uint32_t> m_lineProductIds;

    bool m_updatingTable = false;    // guard against recursive cellChanged

    uint32_t m_id;
    bool     m_isDeleted;
    Invoice  m_result;
    std::vector<InvoiceLine> m_resultLines;
};
