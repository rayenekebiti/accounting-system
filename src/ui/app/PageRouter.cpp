#include "app/PageRouter.h"
#include "app/PageHeader.h"
#include "pages/base/Page.h"
#include <QStackedWidget>

PageRouter::PageRouter(QStackedWidget* stack, PageHeader* header, QObject* parent)
    : QObject(parent), m_stack(stack), m_header(header)
{}

void PageRouter::registerPage(Page* page)
{
    m_pages[page->pageId()] = page;
    m_stack->addWidget(page);
}

void PageRouter::navigateTo(PageId id)
{
    const bool alreadyCurrent = (id == m_currentId && m_stack->currentWidget() != nullptr);
    if (alreadyCurrent) return;

    if (m_stack->currentWidget() != nullptr) {
        if (auto* prev = m_pages.value(m_currentId))
            prev->onDeactivated();
    }

    auto* page = m_pages.value(id, nullptr);
    if (!page) return;

    m_currentId = id;
    m_stack->setCurrentWidget(page);
    m_header->setTitle(page->pageTitle());
    m_header->setActions(page->pageActions());
    page->onActivated();

    emit pageActivated(id);
}
