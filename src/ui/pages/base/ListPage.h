#pragma once
#include "pages/base/Page.h"
#include <QList>

class DataTableView;
class SearchBar;
class FilterBar;
class PaginationFooter;
class PaginationProxy;
class QAbstractProxyModel;
class QVBoxLayout;
class QHBoxLayout;

class ListPage : public Page {
    Q_OBJECT
public:
    explicit ListPage(QWidget* parent = nullptr);

protected:
    DataTableView*    m_table;
    SearchBar*        m_searchBar;
    FilterBar*        m_filterBar;
    PaginationFooter* m_pagination;
    QVBoxLayout*      m_mainLayout;
    QHBoxLayout*      m_filterStrip;
    PaginationProxy*     m_paginationProxy = nullptr;
    QAbstractProxyModel* m_filterProxy     = nullptr;

    // Call this instead of m_table->setModel(proxy) to enable pagination.
    void setupPagination(QAbstractProxyModel* filterProxy);
    // Call after setTotalRecords when filter changes to reset back to page 1.
    void resetToFirstPage();

    void setupListLayout();

    virtual void onSearch(const QString& text) { Q_UNUSED(text) }
    virtual void onFilterChanged()             {}
    virtual void onRowDoubleClicked(int row)   { Q_UNUSED(row) }

private slots:
    void onPageChanged(int page);
    void onPageSizeChanged(int size);
};
