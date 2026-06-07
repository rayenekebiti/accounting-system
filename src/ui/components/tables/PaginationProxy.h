#pragma once
#include <QAbstractProxyModel>

// Wraps any model (typically a filter proxy) and exposes only the current page's rows.
// Chain: source model → FilterProxy → PaginationProxy → QTableView
class PaginationProxy : public QAbstractProxyModel {
    int m_page     = 1;
    int m_pageSize = 25;

public:
    explicit PaginationProxy(QObject* parent = nullptr)
        : QAbstractProxyModel(parent) {}

    void setPage(int page, int pageSize)
    {
        beginResetModel();
        m_page     = qMax(1, page);
        m_pageSize = qMax(1, pageSize);
        endResetModel();
    }

    void setSourceModel(QAbstractItemModel* src) override
    {
        if (sourceModel())
            disconnect(sourceModel(), nullptr, this, nullptr);
        QAbstractProxyModel::setSourceModel(src);
        if (!src) return;
        auto reset = [this] { beginResetModel(); endResetModel(); };
        connect(src, &QAbstractItemModel::modelReset,    this, reset);
        connect(src, &QAbstractItemModel::rowsInserted,  this, reset);
        connect(src, &QAbstractItemModel::rowsRemoved,   this, reset);
        connect(src, &QAbstractItemModel::layoutChanged, this, reset);
        connect(src, &QAbstractItemModel::dataChanged,   this,
            [this](const QModelIndex& tl, const QModelIndex& br, const QList<int>& roles) {
                const QModelIndex ptl = mapFromSource(tl);
                const QModelIndex pbr = mapFromSource(br);
                if (ptl.isValid() && pbr.isValid())
                    emit dataChanged(ptl, pbr, roles);
            });
    }

    int rowCount(const QModelIndex& parent = {}) const override
    {
        if (!sourceModel() || parent.isValid()) return 0;
        const int off   = (m_page - 1) * m_pageSize;
        const int total = sourceModel()->rowCount();
        return qMax(0, qMin(m_pageSize, total - off));
    }

    int columnCount(const QModelIndex& parent = {}) const override
    {
        return sourceModel() ? sourceModel()->columnCount(parent) : 0;
    }

    QModelIndex index(int row, int col, const QModelIndex& parent = {}) const override
    {
        if (parent.isValid()) return {};
        return createIndex(row, col);
    }

    QModelIndex parent(const QModelIndex&) const override { return {}; }

    QModelIndex mapToSource(const QModelIndex& proxyIdx) const override
    {
        if (!proxyIdx.isValid() || !sourceModel()) return {};
        const int off = (m_page - 1) * m_pageSize;
        return sourceModel()->index(proxyIdx.row() + off, proxyIdx.column());
    }

    QModelIndex mapFromSource(const QModelIndex& srcIdx) const override
    {
        if (!srcIdx.isValid()) return {};
        const int off      = (m_page - 1) * m_pageSize;
        const int proxyRow = srcIdx.row() - off;
        if (proxyRow < 0 || proxyRow >= m_pageSize) return {};
        return createIndex(proxyRow, srcIdx.column());
    }

    QVariant headerData(int s, Qt::Orientation o, int role) const override
    {
        return sourceModel() ? sourceModel()->headerData(s, o, role) : QVariant{};
    }
};
