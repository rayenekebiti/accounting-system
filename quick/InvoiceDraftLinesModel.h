#ifndef QUICK_INVOICE_DRAFT_LINES_MODEL_H
#define QUICK_INVOICE_DRAFT_LINES_MODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <vector>
#include "core/InvoiceLine.h"

class InvoiceDraftLinesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        DescriptionRole  = Qt::UserRole + 1,
        QtyTextRole,
        UnitPriceTextRole,
        TaxTextRole,
        LineTotalTextRole
    };
    Q_ENUM(Roles)

    explicit InvoiceDraftLinesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QHash<int, QByteArray> roleNames() const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    Q_INVOKABLE void addBlankLine();
    Q_INVOKABLE void removeLine(int row);
    // Write a cell from QML. Delegates declare `required property` for the roles,
    // which removes the implicit `model` object — so there is no `model.role = v`
    // write-back path; this invokable wraps setData() by field name instead.
    Q_INVOKABLE void setCell(int row, const QString& field, const QString& value);

    void setFromInvoiceLines(const std::vector<InvoiceLine>& lines);
    std::vector<InvoiceLine> buildLines() const;

    double subtotal() const;
    double taxTotal() const;
    double total() const;
    bool hasValidLine() const;

signals:
    void linesChanged();
    void countChanged();

private:
    struct Row {
        uint32_t id = UINT32_MAX;   // STABLE line id; UINT32_MAX = a NEW line (assigned at correction)
        QString  description;
        double   qty;
        double   unitPrice;
        double   taxPct;
    };

    std::vector<Row> rows_;

    static QString formatMoney(double v);
    static double  clamp(double v);
};

#endif // QUICK_INVOICE_DRAFT_LINES_MODEL_H
