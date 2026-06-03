#pragma once
#include <QWidget>

class QLabel;
class QPushButton;
class QComboBox;

class PaginationFooter : public QWidget {
    Q_OBJECT
public:
    explicit PaginationFooter(QWidget* parent = nullptr);

    void setTotalRecords(int total);
    void setCurrentPage(int page);
    void setPageSize(int size);

    int currentPage() const { return m_currentPage; }
    int pageSize()    const { return m_pageSize; }

signals:
    void pageRequested(int page);
    void pageSizeChanged(int size);

private slots:
    void prevPage();
    void nextPage();
    void onPageSizeChanged(int index);

private:
    void updateLabels();

    QLabel*      m_rangeLabel;
    QPushButton* m_prevBtn;
    QPushButton* m_nextBtn;
    QComboBox*   m_pageSizeCombo;

    int m_totalRecords = 0;
    int m_currentPage  = 1;
    int m_pageSize     = 25;
};
