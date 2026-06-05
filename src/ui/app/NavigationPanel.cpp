#include "app/NavigationPanel.h"
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QStyle>
#include <QPainter>
#include <QFontMetrics>

// ── Badge — right-anchored count pill (Dynamics-style) ──────────────────────
class NavBadge : public QWidget {
public:
    static constexpr int kW = 20;
    static constexpr int kH = 16;

    explicit NavBadge(QWidget* parent) : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        setObjectName("__navBadge__");
        setFixedSize(kW, kH);
    }
    void setCount(int n) { m_count = n; update(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_count <= 0) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QString txt = m_count > 99 ? "99+" : QString::number(m_count);
        QFont f("Segoe UI", 8, QFont::Bold);
        p.setFont(f);

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#C83434"));
        p.drawRoundedRect(QRect(0, 0, width(), height()), kH / 2, kH / 2);
        p.setPen(Qt::white);
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter, txt);
    }
private:
    int m_count = 0;
};

// Geometry constants used by both makeNavButton/setBadge and applyCollapsedState
static constexpr int kNavPanelExpanded = 210;
static constexpr int kNavPanelCollapsed = 48;
static constexpr int kNavBtnH = 36;

static QPoint badgePosFor(bool collapsed)
{
    if (collapsed) {
        // Top-right corner of the icon-only button
        return QPoint(kNavPanelCollapsed - NavBadge::kW - 2, 2);
    }
    // Right side, vertically centered on the row
    return QPoint(kNavPanelExpanded - NavBadge::kW - 12,
                  (kNavBtnH - NavBadge::kH) / 2);
}

// ── Nav button stylesheet ─────────────────────────────────────────────────────
static const QString NAV_BTN_STYLE = R"(
QToolButton {
    background: transparent;
    border: none;
    border-left: 2px solid transparent;
    color: #6B7485;
    text-align: left;
    padding: 0px 14px;
    font-size: 13px;
}
QToolButton:hover {
    background: rgba(196,203,216,0.06);
    color: #C4CBD8;
}
QToolButton[active="true"] {
    background: rgba(26,111,224,0.10);
    border-left-color: #1A6FE0;
    color: #C4CBD8;
    font-weight: 600;
}
)";

// ── NavigationPanel ───────────────────────────────────────────────────────────
NavigationPanel::NavigationPanel(QWidget* parent) : QWidget(parent)
{
    setObjectName("navPanel");
    setFixedWidth(210);

    buildEntries();

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Brand header ─────────────────────────────────────────────────────────
    auto* logoBox    = new QWidget(this);
    logoBox->setFixedHeight(52);
    auto* logoLayout = new QHBoxLayout(logoBox);
    logoLayout->setContentsMargins(14, 0, 12, 0);
    logoLayout->setSpacing(10);
    logoLayout->setAlignment(Qt::AlignVCenter);

    auto* monogram = new QLabel("AP", logoBox);
    monogram->setFixedSize(26, 26);
    monogram->setAlignment(Qt::AlignCenter);
    monogram->setStyleSheet(
        "background: #1A6FE0; color: white; border-radius: 4px;"
        " font-size: 11px; font-weight: 700; letter-spacing: 0.5px;");

    auto* appName = new QLabel("AccountingPro", logoBox);
    appName->setStyleSheet(
        "color: #C4CBD8; font-size: 13px; font-weight: 600; background: transparent;");

    logoLayout->addWidget(monogram);
    logoLayout->addWidget(appName);
    logoLayout->addStretch();
    outer->addWidget(logoBox);

    auto* topSep = new QFrame(this);
    topSep->setFrameShape(QFrame::HLine);
    topSep->setFixedHeight(1);
    topSep->setStyleSheet("background: rgba(255,255,255,0.06); border: none;");
    outer->addWidget(topSep);

    outer->addSpacing(6);

    // ── Nav items ─────────────────────────────────────────────────────────────
    m_navLayout = new QVBoxLayout;
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(0);

    auto addSection = [&](const QString& label) {
        auto* lbl = new QLabel(label, this);
        lbl->setFixedHeight(22);
        lbl->setStyleSheet(
            "color: rgba(107,116,133,0.55); font-size: 9px; font-weight: 700;"
            " letter-spacing: 1.4px; padding-left: 18px; background: transparent;");
        m_navLayout->addWidget(lbl);
    };

    auto addSep = [&] {
        m_navLayout->addSpacing(4);
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFixedHeight(1);
        line->setStyleSheet("background: rgba(255,255,255,0.05); border: none;");
        m_navLayout->addWidget(line);
        m_navLayout->addSpacing(4);
    };

    addSection("OPERATIONS");
    for (int i = 0; i <= 5; ++i)
        m_navLayout->addWidget(makeNavButton(m_entries[i]));

    addSep();
    addSection("ANALYTICS");
    m_navLayout->addWidget(makeNavButton(m_entries[6]));

    addSep();
    addSection("SYSTEM");
    m_navLayout->addWidget(makeNavButton(m_entries[7]));

    m_navLayout->addStretch();
    outer->addLayout(m_navLayout, 1);

    // ── Bottom collapse toggle ────────────────────────────────────────────────
    auto* botSep = new QFrame(this);
    botSep->setFrameShape(QFrame::HLine);
    botSep->setFixedHeight(1);
    botSep->setStyleSheet("background: rgba(255,255,255,0.06); border: none;");
    outer->addWidget(botSep);

    m_collapseBtn = new QPushButton("◀  Collapse", this);
    m_collapseBtn->setFixedHeight(36);
    m_collapseBtn->setStyleSheet(
        "QPushButton { color: rgba(107,116,133,0.65); background: transparent;"
        " border: none; text-align: left; padding-left: 18px; font-size: 11px; min-height: 0; }"
        "QPushButton:hover { color: #C4CBD8; background: rgba(196,203,216,0.05); }");
    outer->addWidget(m_collapseBtn);

    connect(m_collapseBtn, &QPushButton::clicked, this, &NavigationPanel::onCollapseToggled);

    if (!m_entries.isEmpty())
        setActivePage(m_entries[0].id);
}

QToolButton* NavigationPanel::makeNavButton(NavEntry& entry)
{
    auto* btn = new QToolButton(this);
    btn->setFixedHeight(36);
    btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn->setStyleSheet(NAV_BTN_STYLE);

    QFont f = btn->font();
    f.setPointSize(11);
    btn->setFont(f);

    btn->setProperty("active", false);
    entry.btn = btn;
    updateButtonText(entry);

    if (entry.badge > 0) {
        auto* badge = new NavBadge(btn);
        badge->setCount(entry.badge);
        badge->move(badgePosFor(m_collapsed));
        badge->show();
    }

    const PageId id = entry.id;
    connect(btn, &QToolButton::clicked, this, [this, id] {
        emit navigationRequested(id);
    });

    return btn;
}

void NavigationPanel::updateButtonText(NavEntry& entry)
{
    if (!entry.btn) return;
    if (m_collapsed)
        entry.btn->setText(entry.icon);
    else
        entry.btn->setText(entry.icon + "  " + entry.label);
}

void NavigationPanel::buildEntries()
{
    m_entries = {
        { PageId::Dashboard, "⊞",  "Dashboard"       },
        { PageId::Customers, "◉",  "Customers"       },
        { PageId::Suppliers, "◈",  "Suppliers"       },
        { PageId::Products,  "◇",  "Products"        },
        { PageId::Invoices,  "≡",  "Invoices",    7  },
        { PageId::Payments,  "≋",  "Payments"        },
        { PageId::Reports,   "↗",  "Reports"         },
        { PageId::Settings,  "⊛",  "Settings"        },
    };
}

void NavigationPanel::setBadge(PageId id, int count)
{
    for (auto& entry : m_entries) {
        if (entry.id != id) continue;
        entry.badge = count;
        if (entry.btn) {
            for (auto* child : entry.btn->findChildren<QWidget*>("__navBadge__"))
                child->deleteLater();
            if (count > 0) {
                auto* badge = new NavBadge(entry.btn);
                badge->setCount(count);
                badge->move(badgePosFor(m_collapsed));
                badge->show();
            }
        }
        break;
    }
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
    setFixedWidth(m_collapsed ? kNavPanelCollapsed : kNavPanelExpanded);
    m_collapseBtn->setText(m_collapsed ? "▶" : "◀  Collapse");

    const QPoint bp = badgePosFor(m_collapsed);
    for (auto& entry : m_entries) {
        if (!entry.btn) continue;

        if (m_collapsed) {
            entry.btn->setText(entry.icon);
            entry.btn->setToolTip(
                entry.badge > 0
                    ? QString("%1 (%2)").arg(entry.label).arg(entry.badge)
                    : entry.label);
        } else {
            updateButtonText(entry);
            entry.btn->setToolTip({});
        }

        for (auto* badge : entry.btn->findChildren<QWidget*>("__navBadge__"))
            badge->move(bp);
    }
}
