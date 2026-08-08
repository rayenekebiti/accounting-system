#ifndef QUICK_TAX_CODE_LIST_MODEL_H
#define QUICK_TAX_CODE_LIST_MODEL_H

#include <QAbstractListModel>
#include <QString>
#include <vector>
#include <cstdint>

// Reads the authoritative, append-only tax policy (AuditJournal::listTaxCodes) — never a
// repository. A code is immutable once authored; a rate change is a new version of its family.
class TaxCodeListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Row {
        uint32_t id           = 0;
        uint16_t family       = 0;
        uint16_t version      = 1;
        uint8_t  type         = 0;
        int32_t  ratePermille = 0;
        QString  name;
        QString  effectiveDate;
    };

    enum Roles {
        TaxCodeIdRole = Qt::UserRole + 1,
        NameRole,
        TypeNameRole,       // "VAT" | "GST" | "Sales Tax" | "Zero-rated" | "Exempt"
        RateTextRole,       // "15.0%"
        VersionRole,
        EffectiveDateRole
    };
    Q_ENUM(Roles)

    explicit TaxCodeListModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    const std::vector<Row>& rows() const { return rows_; }

private:
    std::vector<Row> rows_;
};

#endif // QUICK_TAX_CODE_LIST_MODEL_H
