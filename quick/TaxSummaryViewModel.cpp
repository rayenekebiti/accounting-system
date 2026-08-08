#include "TaxSummaryViewModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include <cstdlib>

static QString money(int64_t cents) { return QString("$%1").arg(std::llabs(cents) / 100.0, 0, 'f', 2); }

TaxSummaryViewModel::TaxSummaryViewModel(QObject* parent)
    : QObject(parent)
    , codes_(new TaxCodeListModel(this))
{
    codes_->refresh();
    recompute();
}

QAbstractItemModel* TaxSummaryViewModel::codesModel() const { return codes_; }
int     TaxSummaryViewModel::codeCount()       const { return codes_->rowCount(); }
QString TaxSummaryViewModel::collectedText()   const { return money(collected_); }
QString TaxSummaryViewModel::recoverableText() const { return money(recoverable_); }
QString TaxSummaryViewModel::netPayableText()  const { return money(netPayable_); }
bool    TaxSummaryViewModel::netIsPayable()    const { return netPayable_ >= 0; }

void TaxSummaryViewModel::refresh()
{
    codes_->refresh();
    recompute();
    emit changed();
}

void TaxSummaryViewModel::recompute()
{
    collected_ = recoverable_ = netPayable_ = 0;
    if (StorageService::instance().isInitialized()) {
        auto& aj = StorageService::instance().audit();
        const auto s = aj.taxSummaryAt(aj.lastSeq());   // reportAt(head): derived from the ledger
        collected_   = s.collected;
        recoverable_ = s.recoverable;
        netPayable_  = s.netPayable;
    }
}
