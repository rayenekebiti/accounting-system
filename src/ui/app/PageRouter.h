#pragma once
#include <QObject>
#include <QMap>
#include "common/UiTypes.h"

class QStackedWidget;
class PageHeader;
class Page;

class PageRouter : public QObject {
    Q_OBJECT
public:
    explicit PageRouter(QStackedWidget* stack, PageHeader* header, QObject* parent = nullptr);

    void navigateTo(PageId id);
    PageId currentPage() const { return m_currentId; }

    void registerPage(Page* page);

signals:
    void pageActivated(PageId id);

private:
    Page* getOrCreate(PageId id);

    QStackedWidget*   m_stack;
    PageHeader*       m_header;
    QMap<PageId, Page*> m_pages;
    PageId m_currentId = static_cast<PageId>(-1);
};
