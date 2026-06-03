#pragma once
#include <QWidget>
#include <QList>
#include <QString>
#include "common/UiTypes.h"

class QAction;

class Page : public QWidget {
    Q_OBJECT
public:
    explicit Page(QWidget* parent = nullptr);
    virtual ~Page() = default;

    virtual PageId          pageId()      const = 0;
    virtual QString         pageTitle()   const = 0;
    virtual QList<QAction*> pageActions() const { return {}; }

    virtual void onActivated()   {}
    virtual void onDeactivated() {}
    virtual void refresh()       {}
};
