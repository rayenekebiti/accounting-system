#ifndef QUICK_EXPENSE_EDITOR_VIEW_MODEL_H
#define QUICK_EXPENSE_EDITOR_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <cstdint>

// Authors an expense through the authoritative engine (AuditJournal::recordExpenseWithPosting /
// recordExpenseVoided) — never a repository write. Create + edit(correct) + void. On edit the
// payment method is fixed (the delta posting keeps the same credit account).
class ExpenseEditorViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int     supplierId    READ supplierId    WRITE setSupplierId    NOTIFY supplierIdChanged)
    Q_PROPERTY(QString date          READ date          WRITE setDate          NOTIFY dateChanged)
    Q_PROPERTY(QString amount        READ amount        WRITE setAmount        NOTIFY amountChanged)
    Q_PROPERTY(int     category      READ category      WRITE setCategory      NOTIFY categoryChanged)
    Q_PROPERTY(int     paymentMethod READ paymentMethod WRITE setPaymentMethod NOTIFY paymentMethodChanged)
    Q_PROPERTY(int     taxCode       READ taxCode       WRITE setTaxCode       NOTIFY taxCodeChanged)
    Q_PROPERTY(QString memo          READ memo          WRITE setMemo          NOTIFY memoChanged)

    Q_PROPERTY(QVariantList supplierOptions READ supplierOptions NOTIFY supplierOptionsChanged)
    Q_PROPERTY(QVariantList categoryOptions READ categoryOptions NOTIFY categoryOptionsChanged)
    Q_PROPERTY(QVariantList taxCodeOptions  READ taxCodeOptions  NOTIFY taxCodeOptionsChanged)

    // Engine-derived tax summary for the editor (net + tax on top = total paid).
    Q_PROPERTY(QString taxText   READ taxText   NOTIFY summaryChanged)
    Q_PROPERTY(QString totalText READ totalText NOTIFY summaryChanged)

    Q_PROPERTY(int  editingId READ editingId NOTIFY editingChanged)
    Q_PROPERTY(bool dirty     READ dirty     NOTIFY dirtyChanged)
    Q_PROPERTY(int  lastExpenseId READ lastExpenseId NOTIFY savedChanged)

    Q_PROPERTY(QString amountError READ amountError NOTIFY validationChanged)
    Q_PROPERTY(QString dateError   READ dateError   NOTIFY validationChanged)
    Q_PROPERTY(bool    valid       READ valid       NOTIFY validationChanged)
    Q_PROPERTY(bool    showErrors  READ showErrors  NOTIFY showErrorsChanged)

public:
    explicit ExpenseEditorViewModel(QObject* parent = nullptr);

    int          supplierId()    const { return supplierId_; }
    QString      date()          const { return date_; }
    QString      amount()        const { return amount_; }
    int          category()      const { return category_; }
    int          paymentMethod() const { return paymentMethod_; }
    int          taxCode()       const { return taxCodeId_; }
    QString      memo()          const { return memo_; }
    QVariantList supplierOptions() const { return supplierOptions_; }
    QVariantList categoryOptions() const;
    QVariantList taxCodeOptions()  const { return taxCodeOptions_; }
    QString      taxText()   const;
    QString      totalText() const;
    int          editingId()     const { return editingId_; }
    bool         dirty()         const { return dirty_; }
    int          lastExpenseId() const { return lastExpenseId_; }

    QString amountError() const { return amountError_; }
    QString dateError()   const { return dateError_; }
    bool    valid()       const { return amountError_.isEmpty() && dateError_.isEmpty(); }
    bool    showErrors()  const { return showErrors_; }

    void setSupplierId(int v);
    void setDate(const QString& v);
    void setAmount(const QString& v);
    void setCategory(int v);
    void setPaymentMethod(int v);
    void setTaxCode(int v);       // resolves the code's rate; recomputes the tax summary
    void setMemo(const QString& v);

    Q_INVOKABLE void beginNew();
    Q_INVOKABLE void beginEdit(int expenseId);
    Q_INVOKABLE bool commit();               // create or correct; lastExpenseId is the id
    Q_INVOKABLE bool voidExpense(int expenseId);
    Q_INVOKABLE void discard();
    // Rebuild the combo option labels ("No tax" / "— none —" / categories) under a newly-installed
    // translator so a live language switch relabels them.
    Q_INVOKABLE void retranslate() { loadTaxCodeOptions(); loadSupplierOptions(); emit categoryOptionsChanged(); }

signals:
    void supplierIdChanged();
    void dateChanged();
    void amountChanged();
    void categoryChanged();
    void paymentMethodChanged();
    void taxCodeChanged();
    void taxCodeOptionsChanged();
    void summaryChanged();
    void memoChanged();
    void supplierOptionsChanged();
    void categoryOptionsChanged();
    void editingChanged();
    void dirtyChanged();
    void savedChanged();
    void showErrorsChanged();
    void validationChanged();

    void saved();
    void discarded();
    void saveFailed(const QString& message);
    void validationFailed(const QString& firstField);

private:
    void revalidate();
    void setDirty(bool v);
    void loadSupplierOptions();
    void loadTaxCodeOptions();

    int     supplierId_    = -1;
    QString date_;
    QString amount_;
    int     category_      = 4;   // Other
    int     paymentMethod_ = 0;   // Cash
    int     taxCodeId_     = -1;  // -1 = no tax
    int     taxRatePermille_ = 0; // resolved from taxCodeId_
    QString memo_;
    QVariantList supplierOptions_;
    QVariantList taxCodeOptions_;

    int     editingId_     = -1;
    int64_t oldAmountCents_ = 0;
    int     oldRatePermille_ = 0;
    bool    dirty_         = false;
    bool    showErrors_    = false;
    int     lastExpenseId_ = -1;

    QString amountError_;
    QString dateError_;
};

#endif // QUICK_EXPENSE_EDITOR_VIEW_MODEL_H
