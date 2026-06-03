#include "app/NavigationPanel.h"
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

NavigationPanel::NavigationPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("navPanel");
    setFixedWidth(220);

    buildEntries();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* logo = new QLabel("  ◈ AccountingPro", this);
    logo->setStyleSheet("color: white; font-size: 15px; font-weight: 700; "
                        "padding: 20px 16px 16px; background: transparent;");
    layout->addWidget(logo);

    m_list = new QListWidget(this);
    m_list->setFocusPolicy(Qt::NoFocus);

    int row = 0;
    for (const auto& entry : m_entries) {
        auto* item = new QListWidgetItem(entry.icon + "  " + entry.label, m_list);
        item->setSizeHint(QSize(-1, 44));
        ++row;
        if (row == 7) {
            auto* sep = new QListWidgetItem(m_list);
            sep->setFlags(Qt::NoItemFlags);
            sep->setSizeHint(QSize(-1, 1));
            sep->setBackground(QColor(55, 71, 79));
        }
    }

    layout->addWidget(m_list, 1);

    m_collapseBtn = new QPushButton("◀  Collapse", this);
    m_collapseBtn->setObjectName("secondary");
    m_collapseBtn->setStyleSheet("QPushButton { color: #9E9E9E; background: transparent; "
                                 "border: none; text-align: left; padding: 12px 16px; border-radius: 0; }");
    layout->addWidget(m_collapseBtn);

    connect(m_list, &QListWidget::currentRowChanged, this, &NavigationPanel::onItemClicked);
    connect(m_collapseBtn, &QPushButton::clicked, this, &NavigationPanel::onCollapseToggled);

    m_list->setCurrentRow(0);
}

void NavigationPanel::buildEntries()
{
    m_entries = {
        { PageId::Dashboard,  "⬛", "Dashboard"  },
        { PageId::Customers,  "👤", "Customers"  },
        { PageId::Suppliers,  "🏭", "Suppliers"  },
        { PageId::Products,   "📦", "Products"   },
        { PageId::Invoices,   "🧾", "Invoices"   },
        { PageId::Payments,   "💵", "Payments"   },
        { PageId::Reports,    "📊", "Reports"    },
        { PageId::Settings,   "⚙", "Settings"   },
    };
}

void NavigationPanel::setActivePage(PageId id)
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].id == id) {
            m_list->blockSignals(true);
            m_list->setCurrentRow(i);
            m_list->blockSignals(false);
            return;
        }
    }
}

void NavigationPanel::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    applyCollapsedState();
}

void NavigationPanel::onItemClicked(int row)
{
    if (row < 0 || row >= m_entries.size()) return;
    emit navigationRequested(m_entries[row].id);
}

void NavigationPanel::onCollapseToggled()
{
    setCollapsed(!m_collapsed);
}

void NavigationPanel::applyCollapsedState()
{
    if (m_collapsed) {
        setFixedWidth(56);
        m_collapseBtn->setText("▶");
        for (int i = 0; i < m_list->count(); ++i) {
            auto* item = m_list->item(i);
            if (item && (item->flags() & Qt::ItemIsEnabled)) {
                const QString full = item->text();
                item->setText(full.left(2));
                item->setToolTip(full.mid(3));
            }
        }
    } else {
        setFixedWidth(220);
        m_collapseBtn->setText("◀  Collapse");
        for (int i = 0; i < m_entries.size(); ++i) {
            if (i < m_list->count()) {
                auto* item = m_list->item(i);
                if (item)
                    item->setText(m_entries[i].icon + "  " + m_entries[i].label);
            }
        }
    }
}
