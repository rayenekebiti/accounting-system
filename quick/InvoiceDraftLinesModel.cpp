#include "InvoiceDraftLinesModel.h"
#include "core/InvoiceLine.h"
#include "core/Money.h"
#include <cmath>
#include <QString>

InvoiceDraftLinesModel::InvoiceDraftLinesModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int InvoiceDraftLinesModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QHash<int, QByteArray> InvoiceDraftLinesModel::roleNames() const
{
    return {
        { DescriptionRole,  "description"   },
        { QtyTextRole,      "qtyText"       },
        { UnitPriceTextRole,"unitPriceText" },
        { TaxTextRole,      "taxText"       },
        { LineTotalTextRole,"lineTotalText" },
    };
}

Qt::ItemFlags InvoiceDraftLinesModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QVariant InvoiceDraftLinesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};

    const Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case DescriptionRole:
        return r.description;
    case QtyTextRole:
        return QString::number(r.qty, 'f', 3);
    case UnitPriceTextRole:
        return QString::number(r.unitPrice, 'f', 2);
    case TaxTextRole:
        return QString::number(r.taxPct, 'f', 1);
    case LineTotalTextRole: {
        double lt = r.unitPrice * r.qty * (1.0 + r.taxPct / 100.0);
        return formatMoney(lt);
    }
    default:
        return {};
    }
}

bool InvoiceDraftLinesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return false;

    Row& r = rows_[static_cast<std::size_t>(index.row())];

    switch (role) {
    case DescriptionRole:
        r.description = value.toString();
        break;
    case QtyTextRole:
        r.qty = clamp(value.toString().toDouble());
        break;
    case UnitPriceTextRole:
        r.unitPrice = clamp(value.toString().toDouble());
        break;
    case TaxTextRole:
        r.taxPct = clamp(value.toString().toDouble());
        break;
    default:
        return false;
    }

    // Notify lineTotal changed for this row
    emit dataChanged(index, index, { LineTotalTextRole });
    emit linesChanged();
    return true;
}

void InvoiceDraftLinesModel::addBlankLine()
{
    const int row = static_cast<int>(rows_.size());
    beginInsertRows(QModelIndex(), row, row);
    rows_.push_back({ UINT32_MAX, QString(), 1.0, 0.0, 0.0 });   // new line → no stable id yet
    endInsertRows();
    emit linesChanged();
    emit countChanged();
}

void InvoiceDraftLinesModel::removeLine(int row)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    beginRemoveRows(QModelIndex(), row, row);
    rows_.erase(rows_.begin() + row);
    endRemoveRows();
    emit linesChanged();
    emit countChanged();
}

void InvoiceDraftLinesModel::setCell(int row, const QString& field, const QString& value)
{
    if (row < 0 || row >= static_cast<int>(rows_.size())) return;
    int role = -1;
    if      (field == QLatin1String("description"))   role = DescriptionRole;
    else if (field == QLatin1String("qtyText"))       role = QtyTextRole;
    else if (field == QLatin1String("unitPriceText")) role = UnitPriceTextRole;
    else if (field == QLatin1String("taxText"))       role = TaxTextRole;
    if (role >= 0)
        setData(index(row), value, role);   // reuses the parsing + notify in setData()
}

void InvoiceDraftLinesModel::setFromInvoiceLines(const std::vector<InvoiceLine>& lines)
{
    beginResetModel();
    rows_.clear();
    for (const InvoiceLine& il : lines) {
        Row r;
        r.id          = il.getId();            // preserve stable line identity across an edit
        r.description = QString::fromUtf8(il.getDescription());
        r.qty         = il.getQuantity();
        r.unitPrice   = il.getUnitPrice().toDouble();
        r.taxPct      = il.getTaxRatePermille() / 10.0;
        rows_.push_back(r);
    }
    endResetModel();
    emit linesChanged();
    emit countChanged();
}

std::vector<InvoiceLine> InvoiceDraftLinesModel::buildLines() const
{
    std::vector<InvoiceLine> result;
    for (const Row& r : rows_) {
        // Keep the UTF-8 buffer alive until InvoiceLine is constructed — its ctor
        // strncpy's from d.description, so a temporary here would dangle.
        const QByteArray descUtf8 = r.description.toUtf8();
        InvoiceLineData d;
        d.description        = descUtf8.constData();
        d.quantityMilliunits = static_cast<int32_t>(std::llround(r.qty * 1000.0));
        d.unitPrice          = Money::fromDouble(r.unitPrice);
        d.taxRatePermille    = static_cast<int16_t>(std::llround(r.taxPct * 10.0));
        InvoiceLine line(d);
        line.recompute();
        line.setId(r.id);   // carry the stable id (UINT32_MAX for new lines → assigned at authoring)
        result.push_back(line);
    }
    return result;
}

double InvoiceDraftLinesModel::subtotal() const
{
    double s = 0.0;
    for (const Row& r : rows_) s += r.qty * r.unitPrice;
    return s;
}

double InvoiceDraftLinesModel::taxTotal() const
{
    double t = 0.0;
    for (const Row& r : rows_) t += r.qty * r.unitPrice * r.taxPct / 100.0;
    return t;
}

double InvoiceDraftLinesModel::total() const
{
    return subtotal() + taxTotal();
}

bool InvoiceDraftLinesModel::hasValidLine() const
{
    for (const Row& r : rows_) {
        if (!r.description.isEmpty() && r.qty > 0.0 && r.unitPrice >= 0.0)
            return true;
    }
    return false;
}

// static helpers
QString InvoiceDraftLinesModel::formatMoney(double v)
{
    return QString("$%1").arg(v, 0, 'f', 2);
}

double InvoiceDraftLinesModel::clamp(double v)
{
    // Non-finite (NaN / ±inf) or negative line input → 0; absurd magnitudes are fenced well
    // inside Money's int64-cents range so a huge qty/price can't overflow the money math.
    if (!std::isfinite(v) || v < 0.0) return 0.0;
    constexpr double kMaxLineValue = 1e12;
    return v > kMaxLineValue ? kMaxLineValue : v;
}
