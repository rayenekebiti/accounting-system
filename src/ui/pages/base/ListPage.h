#pragma once
#include "pages/base/Page.h"
#include <QList>

class DataTableView;
class SearchBar;
class FilterBar;
class PaginationFooter;
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

    void setupListLayout();

    virtual void onSearch(const QString& text) { Q_UNUSED(text) }
    virtual void onFilterChanged()             {}
    virtual void onPageChanged(int page)       { Q_UNUSED(page) }
    virtual void onRowDoubleClicked(int row)   { Q_UNUSED(row) }
};
