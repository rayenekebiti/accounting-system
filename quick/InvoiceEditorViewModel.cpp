#include "InvoiceEditorViewModel.h"
#include "storage/StorageService.h"
#include "core/Invoice.h"
#include "core/InvoiceLine.h"
#include "core/InvoiceTotals.h"
#include "core/IsoDate.h"
#include "core/Money.h"
#include "core/Customer.h"
#include "core/NumberingService.h"

#include <QDate>
#include <QVariantMap>
#include <QRegularExpression>
#include <stdexcept>

static bool isValidIsoDateStr(const QString& s)
{
    static const QRegularExpression re(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}$"));
    if (!re.match(s).hasMatch()) return false;
    auto opt = IsoDate::fromString(s.toStdString());
    return opt.has_value() && opt->isValid();
}

InvoiceEditorViewModel::InvoiceEditorViewModel(QObject* parent)
    : QObject(parent)
    , lines_(new InvoiceDraftLinesModel(this))
{
    // Lines changed → recompute totals + dirty + revalidate
    connect(lines_, &InvoiceDraftLinesModel::linesChanged, this, [this]{
        recomputeTotals();
        // Only mark dirty if we've already finished initializing
        // (setDirty guards the dirty_ == v check so double-calls are no-ops)
        if (dirty_ || isNew_ || editId_ >= 0)
            setDirty(true);
        revalidate();
    });
    connect(lines_, &InvoiceDraftLinesModel::countChanged, this, [this]{
        recomputeTotals();
        revalidate();
    });
}

// ── Setters ──────────────────────────────────────────────────────────────────

void InvoiceEditorViewModel::setInvoiceNumber(const QString& v)
{
    if (invoiceNumber_ == v) return;
    invoiceNumber_ = v;
    setDirty(true);
    revalidate();
    emit invoiceNumberChanged();
}

void InvoiceEditorViewModel::setCustomerId(int v)
{
    if (customerId_ == v) return;
    customerId_ = v;
    setDirty(true);
    revalidate();
    emit customerIdChanged();
}

void InvoiceEditorViewModel::setIssueDate(const QString& v)
{
    if (issueDate_ == v) return;
    issueDate_ = v;
    setDirty(true);
    revalidate();
    emit issueDateChanged();
}

void InvoiceEditorViewModel::setDueDate(const QString& v)
{
    if (dueDate_ == v) return;
    dueDate_ = v;
    setDirty(true);
    revalidate();
    emit dueDateChanged();
}

void InvoiceEditorViewModel::setStatus(int v)
{
    if (status_ == v) return;
    status_ = v;
    setDirty(true);
    emit statusChanged();
}

// ── Derived ──────────────────────────────────────────────────────────────────

QString InvoiceEditorViewModel::subtotalText() const
{
    return formatMoney(lines_->subtotal());
}

QString InvoiceEditorViewModel::taxText() const
{
    return formatMoney(lines_->taxTotal());
}

QString InvoiceEditorViewModel::totalText() const
{
    return formatMoney(lines_->total());
}

// ── Private helpers ───────────────────────────────────────────────────────────

void InvoiceEditorViewModel::resetLinesQuiet()
{
    // Disconnect temporarily to avoid marking dirty during reset
    lines_->disconnect(this);
    lines_->setFromInvoiceLines({});
    // Re-connect
    connect(lines_, &InvoiceDraftLinesModel::linesChanged, this, [this]{
        recomputeTotals();
        if (dirty_ || isNew_ || editId_ >= 0)
            setDirty(true);
        revalidate();
    });
    connect(lines_, &InvoiceDraftLinesModel::countChanged, this, [this]{
        recomputeTotals();
        revalidate();
    });
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void InvoiceEditorViewModel::beginNew()
{
    peekedNumber_ = NumberingService::peekInvoiceNumber();
    invoiceNumber_ = peekedNumber_;
    customerId_    = -1;
    issueDate_     = QDate::currentDate().toString(Qt::ISODate);
    dueDate_       = QDate::currentDate().addDays(30).toString(Qt::ISODate);
    status_        = 0; // INVOICE_DRAFT
    editId_        = -1;
    isNew_         = true;
    dirty_         = false;
    showErrors_    = false;

    // Reset lines quietly then add one blank
    resetLinesQuiet();
    lines_->addBlankLine();
    // After addBlankLine the countChanged fires but we don't want dirty yet
    dirty_ = false;

    loadCustomerOptions();

    revalidate();
    recomputeTotals();

    emit invoiceNumberChanged();
    emit customerIdChanged();
    emit issueDateChanged();
    emit dueDateChanged();
    emit statusChanged();
    emit isNewChanged();
    emit dirtyChanged();
    emit showErrorsChanged();
    emit customerOptionsChanged();
    emit totalsChanged();
    emit validationChanged();
}

void InvoiceEditorViewModel::beginEdit(int invoiceId)
{
    editId_ = invoiceId;
    isNew_  = false;
    dirty_  = false;
    showErrors_ = false;

    auto& storage = StorageService::instance();
    Invoice inv = storage.invoices().load(static_cast<uint32_t>(invoiceId));

    invoiceNumber_ = QString::fromUtf8(inv.getInvoiceNumber());
    customerId_    = static_cast<int>(inv.getCustomerId());
    issueDate_     = QString::fromStdString(inv.getIssueDate().toString());
    dueDate_       = QString::fromStdString(inv.getDueDate().toString());
    status_        = static_cast<int>(inv.getStatus());

    auto lineVec = storage.invoiceLines().findByInvoice(static_cast<uint32_t>(invoiceId));

    // Load lines quietly
    resetLinesQuiet();
    lines_->setFromInvoiceLines(lineVec);
    dirty_ = false;

    loadCustomerOptions();

    revalidate();
    recomputeTotals();

    emit invoiceNumberChanged();
    emit customerIdChanged();
    emit issueDateChanged();
    emit dueDateChanged();
    emit statusChanged();
    emit isNewChanged();
    emit dirtyChanged();
    emit showErrorsChanged();
    emit customerOptionsChanged();
    emit totalsChanged();
    emit validationChanged();
}

// Revenue is recognised only for issued/effective invoices; drafts and voids recognise 0.
// One rule drives the ledger delta across every status transition (see commit()).
static int64_t recognizedCents(int status, int64_t totalCents)
{
    return (status == INVOICE_POSTED || status == INVOICE_OVERDUE || status == INVOICE_PAID)
        ? totalCents : 0;
}

bool InvoiceEditorViewModel::commit()
{
    revalidate();

    if (!valid()) {
        showErrors_ = true;
        emit showErrorsChanged();
        emit validationChanged();
        emit validationFailed(firstInvalidField());
        return false;
    }

    try {
        auto& storage = StorageService::instance();

        // Integrity guard: invoice numbers must be unique. A duplicate would make
        // two distinct invoices indistinguishable in reports and reconciliation.
        const std::string numStd = invoiceNumber_.toStdString();
        const int existingId = storage.invoices().findIdByNumber(numStd.c_str());
        const int selfId = isNew_ ? -1 : editId_;
        if (existingId >= 0 && existingId != selfId) {
            emit saveFailed(QStringLiteral("duplicateNumber"));
            return false;
        }

        Invoice inv;
        inv.setInvoiceNumber(invoiceNumber_.toUtf8().constData());
        inv.setCustomerId(static_cast<uint32_t>(customerId_));

        auto issDate = IsoDate::fromString(issueDate_.toStdString());
        auto dueDate = IsoDate::fromString(dueDate_.toStdString());
        inv.setIssueDate(issDate.value());
        inv.setDueDate(dueDate.value());
        inv.setStatus(static_cast<InvoiceStatus>(status_));

        // Build the lines once, then derive the header totals from the per-line
        // Money so total == Σ lines and subtotal + tax == total hold exactly
        // (see core/InvoiceTotals.h). Never round aggregate doubles here.
        auto builtLines = lines_->buildLines();
        const InvoiceTotals totals = computeInvoiceTotals(builtLines);
        inv.setSubtotal(totals.subtotal);
        inv.setTaxAmount(totals.tax);
        inv.setTotal(totals.total);

        // Ledger deltas = change in recognised NET revenue and OUTPUT TAX (posting-policy v2
        // splits them: Cr Revenue net / Cr Tax Payable tax). Read the OLD projection BEFORE the
        // correction so an edit posts (new − old); a new invoice posts the full amounts.
        int64_t oldNet = 0, oldTax = 0;
        if (!isNew_) {
            const Invoice prev = storage.invoices().load(static_cast<uint32_t>(editId_));
            oldNet = recognizedCents(static_cast<int>(prev.getStatus()), prev.getSubtotal().cents());
            oldTax = recognizedCents(static_cast<int>(prev.getStatus()), prev.getTaxAmount().cents());
            inv.setId(static_cast<uint32_t>(editId_));
        }
        const int64_t netDelta = recognizedCents(status_, totals.subtotal.cents()) - oldNet;
        const int64_t taxDelta = recognizedCents(status_, totals.tax.cents())      - oldTax;

        // ATOMIC business transaction: the invoice (create/correct) AND its ledger revenue+tax
        // posting are authored as ONE indivisible fact (EventLog::appendAtomic) — a crash
        // can never split the operational invoice from its financial interpretation. Routes
        // through the event log; never a direct repo write. Assigns stable invoice + line ids
        // (create) or preserves them + rejects closed-period edits (correct).
        storage.audit().recordInvoiceWithRevenue(inv, builtLines, /*correction*/ !isNew_,
                                                  netDelta, taxDelta, issDate.value(), StorageService::now());
        if (isNew_ && invoiceNumber_ == peekedNumber_)
            NumberingService::reserveInvoiceNumber();

    } catch (const std::exception& e) {
        emit saveFailed(QString::fromUtf8(e.what()));
        return false;
    }

    dirty_ = false;
    emit dirtyChanged();
    emit saved();
    return true;
}

void InvoiceEditorViewModel::discard()
{
    emit discarded();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void InvoiceEditorViewModel::revalidate()
{
    // Error properties expose stable KEYS (not display text); QML maps each to a
    // qsTr() message so translation lives in the presentation layer + retranslates.
    QString custErr, numErr, issErr, dueErr, linesErr;

    if (customerId_ < 0)
        custErr = QStringLiteral("required");

    if (invoiceNumber_.isEmpty())
        numErr = QStringLiteral("required");
    else if (invoiceNumber_.toUtf8().size() > 15)
        numErr = QStringLiteral("tooLong");

    if (!isValidIsoDateStr(issueDate_))
        issErr = QStringLiteral("invalidDate");

    if (!isValidIsoDateStr(dueDate_)) {
        dueErr = QStringLiteral("invalidDate");
    } else if (isValidIsoDateStr(issueDate_)) {
        QDate iss = QDate::fromString(issueDate_, Qt::ISODate);
        QDate due = QDate::fromString(dueDate_,   Qt::ISODate);
        if (due < iss)
            dueErr = QStringLiteral("dueBeforeIssue");
    }

    if (!lines_->hasValidLine())
        linesErr = QStringLiteral("linesRequired");

    bool changed = (customerError_  != custErr)
                || (numberError_    != numErr)
                || (issueDateError_ != issErr)
                || (dueDateError_   != dueErr)
                || (linesError_     != linesErr);

    customerError_  = custErr;
    numberError_    = numErr;
    issueDateError_ = issErr;
    dueDateError_   = dueErr;
    linesError_     = linesErr;

    if (changed) emit validationChanged();
}

void InvoiceEditorViewModel::recomputeTotals()
{
    emit totalsChanged();
}

void InvoiceEditorViewModel::setDirty(bool v)
{
    if (dirty_ == v) return;
    dirty_ = v;
    emit dirtyChanged();
}

void InvoiceEditorViewModel::loadCustomerOptions()
{
    // Cached: rebuilding this list (O(customers), a QVariantMap per customer) on
    // every editor open is wasteful and churns memory. Load once; the screen calls
    // refreshCustomerOptions() when the customer set actually changes.
    if (customerOptionsLoaded_) return;
    refreshCustomerOptions();
}

void InvoiceEditorViewModel::refreshCustomerOptions()
{
    customerOptions_.clear();
    if (StorageService::instance().isInitialized()) {
        for (const Customer& c : StorageService::instance().customers().loadAll()) {
            if (c.getIsDeleted()) continue;
            QVariantMap m;
            m[QStringLiteral("value")] = static_cast<int>(c.getId());
            m[QStringLiteral("label")] = QString::fromUtf8(c.getName());
            customerOptions_.append(m);
        }
    }
    customerOptionsLoaded_ = true;
    emit customerOptionsChanged();
}

QString InvoiceEditorViewModel::firstInvalidField() const
{
    if (!customerError_.isEmpty())  return QStringLiteral("customerId");
    if (!numberError_.isEmpty())    return QStringLiteral("invoiceNumber");
    if (!issueDateError_.isEmpty()) return QStringLiteral("issueDate");
    if (!dueDateError_.isEmpty())   return QStringLiteral("dueDate");
    if (!linesError_.isEmpty())     return QStringLiteral("lines");
    return {};
}

QString InvoiceEditorViewModel::formatMoney(double v)
{
    return QString("$%1").arg(v, 0, 'f', 2);
}
