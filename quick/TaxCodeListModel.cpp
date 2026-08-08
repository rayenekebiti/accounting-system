#include "TaxCodeListModel.h"
#include "storage/StorageService.h"
#include "storage/AuditJournal.h"
#include "core/TaxCode.h"

TaxCodeListModel::TaxCodeListModel(QObject* parent)
    : QAbstractListModel(parent)
{}

int TaxCodeListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant TaxCodeListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(rows_.size()))
        return {};
    const Row& r = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
    case TaxCodeIdRole:     return static_cast<int>(r.id);
    case NameRole:         return r.name;
    case TypeNameRole:     return QString::fromUtf8(taxTypeName(r.type));
    case RateTextRole:     return QString("%1%").arg(r.ratePermille / 10.0, 0, 'f', 1);
    case VersionRole:      return static_cast<int>(r.version);
    case EffectiveDateRole:return r.effectiveDate;
    default:               return {};
    }
}

QHash<int, QByteArray> TaxCodeListModel::roleNames() const
{
    return {
        { TaxCodeIdRole,      "taxCodeId"     },
        { NameRole,           "name"          },
        { TypeNameRole,       "typeName"      },
        { RateTextRole,       "rateText"      },
        { VersionRole,        "version"       },
        { EffectiveDateRole,  "effectiveDate" },
    };
}

void TaxCodeListModel::refresh()
{
    beginResetModel();
    rows_.clear();
    if (StorageService::instance().isInitialized()) {
        for (const auto& c : StorageService::instance().audit().listTaxCodes()) {
            Row r;
            r.id            = c.id;
            r.family        = c.family;
            r.version       = c.version;
            r.type          = c.type;
            r.ratePermille  = c.ratePermille;
            r.name          = QString::fromUtf8(c.name);
            r.effectiveDate = QString::fromStdString(c.effectiveDate.toString());
            rows_.push_back(std::move(r));
        }
    }
    endResetModel();
}
