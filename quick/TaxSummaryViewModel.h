#ifndef QUICK_TAX_SUMMARY_VIEW_MODEL_H
#define QUICK_TAX_SUMMARY_VIEW_MODEL_H

#include <QObject>
#include <QString>
#include <QAbstractItemModel>
#include "TaxCodeListModel.h"

// The Tax workspace tab: the VAT/GST report numbers (collected / recoverable / net payable) —
// all derived from the ledger at the current head (AuditJournal::taxSummaryAt) — plus the
// authoritative tax-code registry. Nothing is cached or computed at reporting time.
class TaxSummaryViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QAbstractItemModel* codesModel READ codesModel CONSTANT)
    Q_PROPERTY(int codeCount READ codeCount NOTIFY changed)

    Q_PROPERTY(QString collectedText   READ collectedText   NOTIFY changed)  // output tax on sales
    Q_PROPERTY(QString recoverableText READ recoverableText NOTIFY changed)  // input tax on purchases
    Q_PROPERTY(QString netPayableText  READ netPayableText  NOTIFY changed)  // owed to the tax authority
    Q_PROPERTY(bool    netIsPayable    READ netIsPayable    NOTIFY changed)  // net ≥ 0 → payable, else refund

public:
    explicit TaxSummaryViewModel(QObject* parent = nullptr);

    QAbstractItemModel* codesModel() const;
    int     codeCount()       const;
    QString collectedText()   const;
    QString recoverableText() const;
    QString netPayableText()  const;
    bool    netIsPayable()    const;

    Q_INVOKABLE void refresh();

signals:
    void changed();

private:
    void recompute();

    TaxCodeListModel* codes_;
    int64_t collected_   = 0;
    int64_t recoverable_ = 0;
    int64_t netPayable_  = 0;
};

#endif // QUICK_TAX_SUMMARY_VIEW_MODEL_H
