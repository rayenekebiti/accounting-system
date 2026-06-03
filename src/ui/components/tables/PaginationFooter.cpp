#include "components/tables/PaginationFooter.h"
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QHBoxLayout>

PaginationFooter::PaginationFooter(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 0);
    layout->setSpacing(8);

    auto* pageSizeLabel = new QLabel("Rows per page:", this);
    pageSizeLabel->setObjectName("muted");

    m_pageSizeCombo = new QComboBox(this);
    for (int n : {10, 25, 50, 100})
        m_pageSizeCombo->addItem(QString::number(n), n);
    m_pageSizeCombo->setCurrentIndex(1);
    m_pageSizeCombo->setFixedWidth(70);

    m_rangeLabel = new QLabel("0–0 of 0", this);
    m_rangeLabel->setObjectName("muted");

    m_prevBtn = new QPushButton("‹ Prev", this);
    m_prevBtn->setObjectName("secondary");
    m_prevBtn->setFixedWidth(72);

    m_nextBtn = new QPushButton("Next ›", this);
    m_nextBtn->setObjectName("secondary");
    m_nextBtn->setFixedWidth(72);

    layout->addWidget(pageSizeLabel);
    layout->addWidget(m_pageSizeCombo);
    layout->addStretch();
    layout->addWidget(m_rangeLabel);
    layout->addWidget(m_prevBtn);
    layout->addWidget(m_nextBtn);

    connect(m_prevBtn, &QPushButton::clicked, this, &PaginationFooter::prevPage);
    connect(m_nextBtn, &QPushButton::clicked, this, &PaginationFooter::nextPage);
    connect(m_pageSizeCombo, &QComboBox::currentIndexChanged,
            this, &PaginationFooter::onPageSizeChanged);
}

void PaginationFooter::setTotalRecords(int total)
{
    m_totalRecords = total;
    m_currentPage  = 1;
    updateLabels();
}

void PaginationFooter::setCurrentPage(int page)
{
    m_currentPage = page;
    updateLabels();
}

void PaginationFooter::setPageSize(int size)
{
    m_pageSize = size;
    updateLabels();
}

void PaginationFooter::prevPage()
{
    if (m_currentPage > 1) {
        --m_currentPage;
        updateLabels();
        emit pageRequested(m_currentPage);
    }
}

void PaginationFooter::nextPage()
{
    const int pages = (m_totalRecords + m_pageSize - 1) / m_pageSize;
    if (m_currentPage < pages) {
        ++m_currentPage;
        updateLabels();
        emit pageRequested(m_currentPage);
    }
}

void PaginationFooter::onPageSizeChanged(int index)
{
    m_pageSize    = m_pageSizeCombo->itemData(index).toInt();
    m_currentPage = 1;
    updateLabels();
    emit pageSizeChanged(m_pageSize);
}

void PaginationFooter::updateLabels()
{
    const int first = m_totalRecords == 0 ? 0 : (m_currentPage - 1) * m_pageSize + 1;
    const int last  = qMin(m_currentPage * m_pageSize, m_totalRecords);
    m_rangeLabel->setText(QString("%1–%2 of %3").arg(first).arg(last).arg(m_totalRecords));

    const int pages = (m_totalRecords + m_pageSize - 1) / m_pageSize;
    m_prevBtn->setEnabled(m_currentPage > 1);
    m_nextBtn->setEnabled(m_currentPage < pages);
}
