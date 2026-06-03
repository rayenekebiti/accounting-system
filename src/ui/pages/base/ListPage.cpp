#include "pages/base/ListPage.h"
#include "components/tables/DataTableView.h"
#include "components/tables/PaginationFooter.h"
#include "components/inputs/SearchBar.h"
#include "components/inputs/FilterBar.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

ListPage::ListPage(QWidget* parent) : Page(parent)
{
    m_table      = new DataTableView(this);
    m_searchBar  = new SearchBar(this);
    m_filterBar  = new FilterBar(this);
    m_pagination = new PaginationFooter(this);
}

void ListPage::setupListLayout()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(24, 16, 24, 16);
    m_mainLayout->setSpacing(16);

    m_filterStrip = new QHBoxLayout;
    m_filterStrip->setSpacing(8);
    m_filterStrip->addWidget(m_searchBar);
    m_filterStrip->addWidget(m_filterBar);
    m_filterStrip->addStretch();

    m_mainLayout->addLayout(m_filterStrip);
    m_mainLayout->addWidget(m_table, 1);
    m_mainLayout->addWidget(m_pagination);

    connect(m_searchBar, &SearchBar::searchChanged, this, &ListPage::onSearch);
    connect(m_filterBar, &FilterBar::filterChanged, this, &ListPage::onFilterChanged);
    connect(m_pagination, &PaginationFooter::pageRequested, this, &ListPage::onPageChanged);
    connect(m_table, &DataTableView::rowDoubleClicked, this, &ListPage::onRowDoubleClicked);
}
