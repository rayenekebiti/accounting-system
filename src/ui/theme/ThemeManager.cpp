#include "theme/ThemeManager.h"
#include <QApplication>

ThemeManager& ThemeManager::instance()
{
    static ThemeManager inst;
    return inst;
}

ThemeManager::ThemeManager(QObject* parent) : QObject(parent) {}

void ThemeManager::apply(QApplication& app, Theme theme)
{
    m_theme = theme;
    app.setStyleSheet(buildStyleSheet(theme));
}

void ThemeManager::setTheme(Theme theme)
{
    if (m_theme == theme) return;
    m_theme = theme;
    if (auto* app = qApp)
        app->setStyleSheet(buildStyleSheet(theme));
    emit themeChanged(theme);
}

QString ThemeManager::accentColor() const
{
    return m_theme == Theme::Dark ? "#42A5F5" : "#1976D2";
}

QString ThemeManager::buildStyleSheet(Theme theme) const
{
    const bool dark = (theme == Theme::Dark);

    // Color palette
    const QString bg        = dark ? "#121212" : "#1F2937";
    const QString surface   = dark ? "#1E1E1E" : "#FFFFFF";
    const QString surface2  = dark ? "#252525" : "#FAFAFA";
    const QString border    = dark ? "#2E2E2E" : "#E2E6EA";
    const QString borderFoc = dark ? "#42A5F5" : "#1976D2";
    const QString text       = dark ? "#E8EAED" : "#1A1A2E";
    const QString textSec    = dark ? "#9AA0A6" : "#5F6368";
    const QString accent     = dark ? "#42A5F5" : "#1976D2";
    const QString accentH    = dark ? "#1E88E5" : "#1565C0";
    const QString accentL    = dark ? "rgba(66,165,245,0.12)" : "rgba(25,118,210,0.08)";
    const QString navBg      = dark ? "#0F0F0F" : "#1A2332";
    const QString danger     = "#D32F2F";
    const QString success    = "#388E3C";
    const QString warning    = "#F57C00";

    QString ss;

    // Base font
    ss += "* { font-family: 'Segoe UI', 'Noto Color Emoji', system-ui, sans-serif;"
          " font-size: 13px; outline: none; }\n";

    // Root backgrounds
    ss += QString("QWidget  { background: %1; color: %2; }\n").arg(bg, text);
    ss += QString("QMainWindow { background: %1; }\n").arg(bg);
    ss += QString("QDialog  { background: %1; }\n").arg(surface);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ss += QString("QToolBar { background: %1; border: none;"
                  " border-bottom: 1px solid %2; spacing: 8px; padding: 4px 12px; }\n")
              .arg(surface, border);
    ss += QString("QToolBar QToolButton { background: transparent; border: 1px solid transparent;"
                  " border-radius: 6px; padding: 5px 12px; color: %1; font-size: 13px; }\n").arg(text);
    ss += QString("QToolBar QToolButton:hover { background: %1; border-color: %2; }\n")
              .arg(accentL, border);
    ss += QString("QToolBar QToolButton:pressed { background: %1; }\n").arg(accentL);
    ss += QString("QToolBar::separator { background: %1; width: 1px; margin: 6px 4px; }\n").arg(border);

    // ── Inputs ───────────────────────────────────────────────────────────────
    const QString inputBase = QString(
        "background: %1; border: 1.5px solid %2; border-radius: 6px;"
        " padding: 5px 10px; color: %3; selection-background-color: %4;")
            .arg(surface, border, text, accentL);

    ss += QString("QLineEdit { %1 }\n").arg(inputBase);
    ss += QString("QLineEdit:focus { border-color: %1; background: %2; }\n").arg(borderFoc, surface);
    ss += QString("QLineEdit:hover:!focus { border-color: #BDBDBD; }\n");

    ss += QString("QComboBox { %1 min-width: 80px; }\n").arg(inputBase);
    ss += QString("QComboBox:focus { border-color: %1; }\n").arg(borderFoc);
    ss += QString("QComboBox:hover:!focus { border-color: #BDBDBD; }\n");
    ss += "QComboBox::drop-down { border: none; width: 22px; subcontrol-position: right center; }\n";
    ss += "QComboBox::down-arrow { width: 10px; height: 10px; }\n";
    ss += QString("QComboBox QAbstractItemView { background: %1; border: 1px solid %2;"
                  " border-radius: 6px; selection-background-color: %3;"
                  " selection-color: %4; outline: none; }\n")
              .arg(surface, border, accentL, text);

    ss += QString("QSpinBox, QDoubleSpinBox { %1 }\n").arg(inputBase);
    ss += QString("QSpinBox:focus, QDoubleSpinBox:focus { border-color: %1; }\n").arg(borderFoc);
    ss += "QSpinBox::up-button, QSpinBox::down-button,"
          " QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border: none; width: 18px; }\n";

    ss += QString("QDateEdit { %1 }\n").arg(inputBase);
    ss += QString("QDateEdit:focus { border-color: %1; }\n").arg(borderFoc);
    ss += "QDateEdit::drop-down { border: none; width: 22px; }\n";

    ss += QString("QPlainTextEdit { %1 padding: 8px; }\n").arg(inputBase);
    ss += QString("QPlainTextEdit:focus { border-color: %1; }\n").arg(borderFoc);

    // ── Buttons ──────────────────────────────────────────────────────────────
    ss += QString("QPushButton { background: %1; color: white; border: none;"
                  " border-radius: 6px; padding: 7px 18px; font-weight: 600;"
                  " font-size: 13px; min-height: 32px; }\n").arg(accent);
    ss += QString("QPushButton:hover { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton:pressed { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton:disabled { background: %1; color: %2; }\n").arg(border, textSec);

    ss += QString("QPushButton#secondary { background: transparent; color: %1;"
                  " border: 1.5px solid %1; }\n").arg(accent);
    ss += QString("QPushButton#secondary:hover { background: %1; }\n").arg(accentL);

    ss += QString("QPushButton#danger { background: %1; color: white; }\n").arg(danger);
    ss += QString("QPushButton#danger:hover { background: #B71C1C; }\n");

    ss += QString("QPushButton#flat { background: transparent; color: %1; border: none; }\n").arg(accent);
    ss += QString("QPushButton#flat:hover { background: %1; }\n").arg(accentL);

    // ── Tables ───────────────────────────────────────────────────────────────
    ss += QString("QTableView { background: %1; border: 1px solid %2; border-radius: 8px;"
                  " gridline-color: transparent; alternate-background-color: %3;"
                  " selection-background-color: %4; selection-color: %5; }\n")
              .arg(surface, border, surface2, accentL, text);
    ss += "QTableView::item { padding: 8px 10px; border: none; border-bottom: 1px solid "
          + border + "; }\n";
    ss += QString("QTableView::item:selected { background: %1; color: %2; }\n").arg(accentL, text);
    ss += QString("QTableView::item:hover { background: %1; }\n").arg(surface2);

    ss += QString("QHeaderView { background: %1; border: none; }\n").arg(surface);
    ss += QString("QHeaderView::section { background: %1; border: none;"
                  " border-bottom: 2px solid %2; border-right: 1px solid %3;"
                  " padding: 8px 10px; font-weight: 700; font-size: 12px;"
                  " color: %4; text-transform: uppercase; letter-spacing: 0.5px; }\n")
              .arg(surface, accent, border, textSec);
    ss += "QHeaderView::section:last { border-right: none; }\n";

    // ── Scrollbars ───────────────────────────────────────────────────────────
    ss += "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; border: none; }\n";
    ss += QString("QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 24px; }\n").arg(border);
    ss += QString("QScrollBar::handle:vertical:hover { background: %1; }\n").arg(textSec);
    ss += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n";
    ss += "QScrollBar:horizontal { height: 6px; background: transparent; margin: 0; border: none; }\n";
    ss += QString("QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 24px; }\n").arg(border);
    ss += QString("QScrollBar::handle:horizontal:hover { background: %1; }\n").arg(textSec);
    ss += "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n";

    // ── Sidebar (NavigationPanel) ─────────────────────────────────────────────
    ss += QString("QWidget#navPanel { background: %1; }\n").arg(navBg);

    // ── Page header ──────────────────────────────────────────────────────────
    ss += QString("QWidget#pageHeaderWidget { background: %1;"
                  " border-bottom: 1px solid %2; }\n").arg(surface, border);

    // ── Labels ───────────────────────────────────────────────────────────────
    ss += "QLabel { background: transparent; }\n";
    ss += QString("QLabel#muted    { color: %1; font-size: 12px; }\n").arg(textSec);
    ss += QString("QLabel#pageTitle{ font-size: 18px; font-weight: 700; color: %1; }\n").arg(text);
    ss += QString("QLabel#sectionTitle { font-size: 13px; font-weight: 700;"
                  " color: %1; letter-spacing: 0.3px; }\n").arg(text);
    ss += QString("QLabel#kpiTitle { font-size: 11px; font-weight: 600;"
                  " color: %1; text-transform: uppercase; letter-spacing: 0.8px; }\n").arg(textSec);
    ss += QString("QLabel#kpiValue { font-size: 26px; font-weight: 700; color: %1; }\n").arg(text);

    // ── Cards / Frames ───────────────────────────────────────────────────────
    ss += QString("QFrame#card { background: %1; border: 1px solid %2;"
                  " border-radius: 10px; }\n").arg(surface, border);
    ss += QString("QFrame#kpiCard { background: %1; border: 1px solid %2;"
                  " border-radius: 10px; }\n").arg(surface, border);

    // ── Splitter ──────────────────────────────────────────────────────────────
    ss += QString("QSplitter::handle { background: %1; }\n").arg(border);
    ss += "QSplitter::handle:horizontal { width: 1px; }\n";
    ss += "QSplitter::handle:vertical   { height: 1px; }\n";

    // ── Tabs ──────────────────────────────────────────────────────────────────
    ss += "QTabBar { background: transparent; border: none; }\n";
    ss += "QTabBar::tab { background: transparent; border: none;"
          " border-bottom: 2px solid transparent; padding: 8px 20px;"
          " margin-right: 4px; font-size: 13px; }\n";
    ss += QString("QTabBar::tab:selected { color: %1; border-bottom-color: %1;"
                  " font-weight: 700; }\n").arg(accent);
    ss += QString("QTabBar::tab:!selected { color: %1; }\n").arg(textSec);
    ss += "QTabBar::tab:hover:!selected { background: rgba(128,128,128,0.06); border-radius: 4px 4px 0 0; }\n";
    ss += "QTabWidget::pane { border: none; border-top: 1px solid " + border + "; }\n";

    // ── Status bar ────────────────────────────────────────────────────────────
    ss += QString("QStatusBar { background: %1; border-top: 1px solid %2;"
                  " color: %3; font-size: 12px; padding: 0 8px; }\n")
              .arg(surface, border, textSec);
    ss += "QStatusBar::item { border: none; padding: 0 6px; }\n";

    // ── ListWidget (report catalog, settings categories) ──────────────────────
    ss += QString("QListWidget { background: %1; border: none; outline: none; }\n").arg(surface2);
    ss += QString("QListWidget::item { padding: 8px 16px; border-radius: 6px;"
                  " margin: 1px 4px; color: %1; }\n").arg(text);
    ss += QString("QListWidget::item:hover { background: %1; }\n").arg(accentL);
    ss += QString("QListWidget::item:selected { background: %1; color: %2;"
                  " font-weight: 600; }\n").arg(accentL, accent);

    // ── Group box ─────────────────────────────────────────────────────────────
    ss += QString("QGroupBox { border: 1px solid %1; border-radius: 8px;"
                  " margin-top: 14px; padding-top: 10px; }\n").arg(border);
    ss += QString("QGroupBox::title { color: %1; font-weight: 700; font-size: 12px;"
                  " subcontrol-origin: margin; left: 10px; }\n").arg(textSec);

    // ── Dialog button box ────────────────────────────────────────────────────
    ss += "QDialogButtonBox { button-layout: 2; }\n";

    // ── CheckBox / RadioButton ────────────────────────────────────────────────
    ss += QString("QCheckBox { spacing: 8px; color: %1; }\n").arg(text);
    ss += QString("QCheckBox::indicator { width: 16px; height: 16px; border-radius: 3px;"
                  " border: 1.5px solid %1; background: %2; }\n").arg(border, surface);
    ss += QString("QCheckBox::indicator:checked { background: %1; border-color: %1; }\n").arg(accent);
    ss += QString("QRadioButton { spacing: 8px; color: %1; }\n").arg(text);

    // ── ScrollArea ────────────────────────────────────────────────────────────
    ss += "QScrollArea { border: none; background: transparent; }\n";
    ss += "QScrollArea > QWidget > QWidget { background: transparent; }\n";

    // ── Menu ──────────────────────────────────────────────────────────────────
    ss += QString("QMenu { background: %1; border: 1px solid %2; border-radius: 8px;"
                  " padding: 4px; }\n").arg(surface, border);
    ss += QString("QMenu::item { padding: 8px 16px; border-radius: 4px; color: %1; }\n").arg(text);
    ss += QString("QMenu::item:selected { background: %1; }\n").arg(accentL);
    ss += "QMenu::separator { height: 1px; background: " + border + "; margin: 4px 8px; }\n";

    return ss;
}
