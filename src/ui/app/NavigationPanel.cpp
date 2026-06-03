#include "app/NavigationPanel.h"
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QStyle>

static const QString NAV_BTN_STYLE = R"(
QToolButton {
    background: transparent;
    border: none;
    border-left: 3px solid transparent;
    color: #B0BEC5;
    text-align: left;
    padding: 0px 16px;
    font-size: 13px;
}
QToolButton:hover {
    background: rgba(255,255,255,0.07);
    color: #ECEFF1;
}
QToolButton[active="true"] {
    background: rgba(255,255,255,0.10);
    border-left-color: #42A5F5;
    color: #FFFFFF;
    font-weight: 600;
}
)";

NavigationPanel::NavigationPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("navPanel");
    setFixedWidth(228);

    buildEntries();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // Logo / app name
    auto* logo = new QLabel("  ◈  AccountingPro", this);
    logo->setFixedHeight(60);
    logo->setStyleSheet("color: white; font-size: 14px; font-weight: 700; "
                        "padding-left: 16px; background: transparent; letter-spacing: 0.5px;");
    outerLayout->addWidget(logo);

    // Thin separator
    auto* topSep = new QFrame(this);
    topSep->setFrameShape(QFrame::HLine);
    topSep->setFixedHeight(1);
    topSep->setStyleSheet("background: rgba(255,255,255,0.08); border: none;");
    outerLayout->addWidget(topSep);

    outerLayout->addSpacing(8);

    // Nav buttons area (scrollable internally via layout)
    m_navLayout = new QVBoxLayout;
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(2);

    // Group label: Operations
    auto* opsLabel = new QLabel("  MAIN MENU", this);
    opsLabel->setFixedHeight(28);
    opsLabel->setStyleSheet("color: rgba(176,190,197,0.5); font-size: 10px; "
                            "font-weight: 700; letter-spacing: 1px; padding-left: 20px; "
                            "background: transparent;");
    m_navLayout->addWidget(opsLabel);

    for (int i = 0; i < m_entries.size(); ++i) {
        if (i == 7) {
            // Separator before Settings
            m_navLayout->addSpacing(8);
            auto* sep = new QFrame(this);
            sep->setFrameShape(QFrame::HLine);
            sep->setFixedHeight(1);
            sep->setStyleSheet("background: rgba(255,255,255,0.08); border: none;");
            m_navLayout->addWidget(sep);
            m_navLayout->addSpacing(8);

            auto* sysLabel = new QLabel("  SYSTEM", this);
            sysLabel->setFixedHeight(28);
            sysLabel->setStyleSheet("color: rgba(176,190,197,0.5); font-size: 10px; "
                                    "font-weight: 700; letter-spacing: 1px; padding-left: 20px; "
                                    "background: transparent;");
            m_navLayout->addWidget(sysLabel);
        }
        m_navLayout->addWidget(makeNavButton(m_entries[i]));
    }

    m_navLayout->addStretch();

    outerLayout->addLayout(m_navLayout, 1);

    // Bottom separator + collapse
    auto* botSep = new QFrame(this);
    botSep->setFrameShape(QFrame::HLine);
    botSep->setFixedHeight(1);
    botSep->setStyleSheet("background: rgba(255,255,255,0.08); border: none;");
    outerLayout->addWidget(botSep);

    m_collapseBtn = new QPushButton("◀   Collapse", this);
    m_collapseBtn->setFixedHeight(44);
    m_collapseBtn->setStyleSheet(
        "QPushButton { color: rgba(176,190,197,0.7); background: transparent; "
        "border: none; text-align: left; padding-left: 20px; font-size: 12px; }"
        "QPushButton:hover { color: #ECEFF1; background: rgba(255,255,255,0.06); }");
    outerLayout->addWidget(m_collapseBtn);

    connect(m_collapseBtn, &QPushButton::clicked, this, &NavigationPanel::onCollapseToggled);

    // Activate first entry
    if (!m_entries.isEmpty())
        setActivePage(m_entries[0].id);
}

QToolButton* NavigationPanel::makeNavButton(NavEntry& entry)
{
    auto* btn = new QToolButton(this);
    btn->setFixedHeight(44);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setStyleSheet(NAV_BTN_STYLE);

    // Use a two-label layout inside a container widget via a trick:
    // Set the button text as "EMOJI  Label" with a rich font approach
    // We'll use a custom widget approach using a child layout
    btn->setText(entry.icon + "   " + entry.label);

    // Make icon part bigger via font override on the text
    QFont f = btn->font();
    f.setPointSize(12);
    btn->setFont(f);

    btn->setProperty("active", false);
    entry.btn = btn;

    const PageId id = entry.id;
    connect(btn, &QToolButton::clicked, this, [this, id] {
        emit navigationRequested(id);
    });

    return btn;
}

void NavigationPanel::buildEntries()
{
    m_entries = {
        { PageId::Dashboard, "🏠", "Dashboard"  },
        { PageId::Customers, "👥", "Customers"  },
        { PageId::Suppliers, "🏢", "Suppliers"  },
        { PageId::Products,  "🛍", "Products"   },
        { PageId::Invoices,  "🧾", "Invoices"   },
        { PageId::Payments,  "💳", "Payments"   },
        { PageId::Reports,   "📈", "Reports"    },
        { PageId::Settings,  "⚙", "Settings"   },
    };
}

void NavigationPanel::setActivePage(PageId id)
{
    for (auto& entry : m_entries) {
        const bool active = (entry.id == id);
        if (entry.btn) {
            entry.btn->setProperty("active", active);
            entry.btn->style()->unpolish(entry.btn);
            entry.btn->style()->polish(entry.btn);
        }
    }
}

void NavigationPanel::setCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
    applyCollapsedState();
}

void NavigationPanel::onCollapseToggled()
{
    setCollapsed(!m_collapsed);
}

void NavigationPanel::applyCollapsedState()
{
    if (m_collapsed) {
        setFixedWidth(64);
        m_collapseBtn->setText("▶");
        for (auto& entry : m_entries) {
            if (entry.btn) {
                entry.btn->setText(entry.icon);
                entry.btn->setToolTip(entry.label);
            }
        }
    } else {
        setFixedWidth(228);
        m_collapseBtn->setText("◀   Collapse");
        for (auto& entry : m_entries) {
            if (entry.btn)
                entry.btn->setText(entry.icon + "   " + entry.label);
        }
    }
}
