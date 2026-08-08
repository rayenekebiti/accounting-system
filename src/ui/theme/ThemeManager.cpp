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
    return "#4C8DFF";
}

QString ThemeManager::buildStyleSheet(Theme theme) const
{
    const bool dark = (theme == Theme::Dark);

    // Modern minimal palette — soft neutral surfaces, single blue accent
    const QString bg       = dark ? "#101218"                 : "#F6F7F9";
    const QString surface  = dark ? "#161A22"                 : "#FFFFFF";
    const QString surface2 = dark ? "#1C212B"                 : "#F2F4F7";
    const QString surface3 = dark ? "#242A36"                 : "#EAEDF2";
    const QString surface4 = dark ? "#2D3442"                 : "#DFE3EA";
    const QString border   = dark ? "#252B37"                 : "#E6E9EF";
    const QString borderH  = dark ? "#3A4250"                 : "#C7CDD9";
    const QString text     = dark ? "#E7EAF0"                 : "#1A2233";
    const QString textSec  = dark ? "#98A2B3"                 : "#5E6A80";
    const QString textMute = dark ? "#5A6375"                 : "#9AA3B5";
    const QString accent   = "#4C8DFF";
    const QString accentH  = dark ? "#6BA1FF"                 : "#3D7BF0";
    const QString accentL  = dark ? "rgba(76,141,255,0.14)"   : "rgba(76,141,255,0.10)";
    const QString navBg    = dark ? "#0C0E13"                 : "#14171E";
    const QString success  = "#2EB67D";
    const QString warning  = "#E2A03F";
    const QString danger   = "#E5534B";

    QString ss;

    ss += "* { font-family: 'Segoe UI Variable Text', 'Segoe UI', system-ui, sans-serif;"
          " font-size: 13px; outline: none; }\n";

    ss += QString("QWidget     { background: %1; color: %2; }\n").arg(bg, text);
    ss += QString("QMainWindow { background: %1; }\n").arg(bg);
    ss += QString("QDialog     { background: %1; }\n").arg(surface);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ss += QString("QToolBar { background: %1; border: none;"
                  " border-bottom: 1px solid %2; spacing: 6px; padding: 6px 14px; }\n")
              .arg(surface, border);
    ss += QString("QToolBar QToolButton { background: transparent; border: 1px solid transparent;"
                  " border-radius: 6px; padding: 5px 10px; color: %1; font-size: 13px; }\n").arg(text);
    ss += QString("QToolBar QToolButton:hover { background: %1; }\n").arg(surface3);
    ss += QString("QToolBar QToolButton:pressed { background: %1; }\n").arg(surface4);
    ss += QString("QToolBar::separator { background: %1; width: 1px; margin: 6px 8px; }\n").arg(border);

    // Primary toolbar action ("+ New")
    ss += QString("QToolButton#primaryAction { background: %1; color: white; border: none;"
                  " border-radius: 6px; padding: 6px 16px; font-weight: 600; font-size: 13px; }\n").arg(accent);
    ss += QString("QToolButton#primaryAction:hover { background: %1; color: white; }\n").arg(accentH);
    ss += "QToolButton#primaryAction::menu-indicator { image: none; }\n";

    // Page-header action buttons
    ss += QString("QToolButton#headerAction { background: transparent; border: 1px solid %1;"
                  " border-radius: 6px; padding: 5px 14px; color: %2; font-weight: 500; }\n")
              .arg(border, text);
    ss += QString("QToolButton#headerAction:hover { background: %1; border-color: %2; color: %3; }\n")
              .arg(accentL, accent, accent);

    // ── Inputs ───────────────────────────────────────────────────────────────
    const QString inputBase = QString(
        "background: %1; border: 1px solid %2; border-radius: 6px;"
        " padding: 6px 12px; color: %3; selection-background-color: %4;")
            .arg(surface2, border, text, accentL);

    ss += QString("QLineEdit { %1 }\n").arg(inputBase);
    ss += QString("QLineEdit:focus { border-color: %1; background: %2; }\n").arg(accent, surface);
    ss += QString("QLineEdit:hover:!focus { border-color: %1; }\n").arg(borderH);

    ss += QString("QComboBox { %1 min-width: 80px; }\n").arg(inputBase);
    ss += QString("QComboBox:focus { border-color: %1; }\n").arg(accent);
    ss += QString("QComboBox:hover:!focus { border-color: %1; }\n").arg(borderH);
    ss += "QComboBox::drop-down { border: none; width: 22px; subcontrol-position: right center; }\n";
    ss += QString("QComboBox::down-arrow { border-left: 4px solid transparent;"
                  " border-right: 4px solid transparent;"
                  " border-top: 5px solid %1; width: 0; height: 0; }\n").arg(textSec);
    ss += QString("QComboBox QAbstractItemView { background: %1; border: 1px solid %2;"
                  " border-radius: 8px; selection-background-color: %3;"
                  " selection-color: %4; outline: none; padding: 4px; }\n")
              .arg(surface, border, accentL, text);

    ss += QString("QSpinBox, QDoubleSpinBox { %1 }\n").arg(inputBase);
    ss += QString("QSpinBox:focus, QDoubleSpinBox:focus { border-color: %1; }\n").arg(accent);
    ss += "QSpinBox::up-button, QSpinBox::down-button,"
          " QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border: none; width: 16px; }\n";

    ss += QString("QDateEdit { %1 }\n").arg(inputBase);
    ss += QString("QDateEdit:focus { border-color: %1; }\n").arg(accent);
    ss += "QDateEdit::drop-down { border: none; width: 22px; }\n";

    ss += QString("QPlainTextEdit { %1 padding: 10px; }\n").arg(inputBase);
    ss += QString("QPlainTextEdit:focus { border-color: %1; }\n").arg(accent);

    // ── Buttons ──────────────────────────────────────────────────────────────
    ss += QString("QPushButton { background: %1; color: white; border: none;"
                  " border-radius: 6px; padding: 6px 18px; font-weight: 600;"
                  " font-size: 13px; min-height: 30px; }\n").arg(accent);
    ss += QString("QPushButton:hover   { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton:pressed { background: %1; }\n").arg(accent);
    ss += QString("QPushButton:disabled{ background: %1; color: %2; }\n").arg(surface3, textMute);

    ss += QString("QPushButton#secondary { background: %1; color: %2;"
                  " border: 1px solid %3; }\n").arg(surface, text, border);
    ss += QString("QPushButton#secondary:hover { background: %1; border-color: %2; }\n")
              .arg(surface3, borderH);
    ss += QString("QPushButton#secondary:disabled { background: transparent; color: %1;"
                  " border-color: %2; }\n").arg(textMute, border);

    ss += QString("QPushButton#danger { background: %1; color: white; border: none; }\n").arg(danger);
    ss += "QPushButton#danger:hover { background: #D3433B; }\n";

    ss += QString("QPushButton#flat { background: transparent; color: %1; border: none; min-height: 0; }\n").arg(textSec);
    ss += QString("QPushButton#flat:hover { color: %1; background: %2; }\n").arg(text, surface3);

    // ── Tables ───────────────────────────────────────────────────────────────
    ss += QString("QTableView { background: %1; border: none;"
                  " alternate-background-color: %1;"
                  " selection-background-color: %2; selection-color: %3; }\n")
              .arg(surface, accentL, text);
    ss += QString("QTableView::item { padding: 0px 12px; border: none;"
                  " border-bottom: 1px solid %1; }\n").arg(border);
    ss += QString("QTableView::item:selected { background: %1; color: %2; }\n").arg(accentL, text);
    ss += QString("QTableView::item:hover:!selected { background: %1; }\n").arg(surface2);

    // Header — flat, quiet, hairline separator only
    ss += QString("QHeaderView { background: %1; border: none; }\n").arg(surface);
    ss += QString("QHeaderView::section { background: %1; border: none;"
                  " border-bottom: 1px solid %2;"
                  " padding: 0px 12px; height: 34px;"
                  " font-size: 11px; font-weight: 600; color: %3;"
                  " letter-spacing: 0.5px; }\n")
              .arg(surface, border, textSec);
    // Suppress Qt's built-in sort arrows — avoids the "^" doubling artifact
    ss += "QHeaderView::up-arrow, QHeaderView::down-arrow { image: none; width: 0; height: 0; }\n";

    // ── Scrollbars ───────────────────────────────────────────────────────────
    ss += "QScrollBar:vertical { width: 8px; background: transparent; margin: 2px; border: none; }\n";
    ss += QString("QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }\n").arg(surface4);
    ss += QString("QScrollBar::handle:vertical:hover { background: %1; }\n").arg(textMute);
    ss += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n";
    ss += "QScrollBar:horizontal { height: 8px; background: transparent; margin: 2px; border: none; }\n";
    ss += QString("QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 30px; }\n").arg(surface4);
    ss += QString("QScrollBar::handle:horizontal:hover { background: %1; }\n").arg(textMute);
    ss += "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n";

    // ── Sidebar ───────────────────────────────────────────────────────────────
    ss += QString("QWidget#navPanel { background: %1; }\n").arg(navBg);

    // ── Page header ──────────────────────────────────────────────────────────
    ss += QString("QWidget#pageHeaderWidget { background: %1;"
                  " border-bottom: 1px solid %2; }\n").arg(bg, border);

    // ── Labels ───────────────────────────────────────────────────────────────
    ss += "QLabel { background: transparent; }\n";
    ss += QString("QLabel#muted       { color: %1; font-size: 12px; }\n").arg(textSec);
    ss += QString("QLabel#pageTitle   { font-size: 17px; font-weight: 700; color: %1; }\n").arg(text);
    ss += QString("QLabel#sectionTitle { font-size: 13px; font-weight: 600;"
                  " color: %1; }\n").arg(text);
    ss += QString("QLabel#kpiTitle  { font-size: 12px; font-weight: 500; color: %1; }\n").arg(textSec);
    ss += QString("QLabel#kpiValue  { font-size: 24px; font-weight: 700; color: %1; }\n").arg(text);
    ss += QString("QLabel#statValue { font-size: 13px; font-weight: 600; color: %1; }\n").arg(text);

    // ── Cards / Frames ───────────────────────────────────────────────────────
    ss += QString("QFrame#card { background: %1; border: 1px solid %2;"
                  " border-radius: 10px; }\n").arg(surface, border);
    ss += QString("QFrame#dangerCard { background: %1; border: 1px solid %2;"
                  " border-left: 3px solid %3; border-radius: 10px; }\n")
              .arg(surface, border, danger);
    ss += "QFrame#kpiCard { background: transparent; border: none; }\n";

    // ── Splitter ──────────────────────────────────────────────────────────────
    ss += QString("QSplitter::handle { background: %1; }\n").arg(border);
    ss += "QSplitter::handle:horizontal { width: 1px; }\n";
    ss += "QSplitter::handle:vertical   { height: 1px; }\n";

    // ── Tabs ──────────────────────────────────────────────────────────────────
    ss += "QTabBar { background: transparent; border: none; }\n";
    ss += QString("QTabBar::tab { background: transparent; border: none;"
                  " border-bottom: 2px solid transparent; padding: 8px 16px;"
                  " margin-right: 4px; font-size: 13px; color: %1; }\n").arg(textSec);
    ss += QString("QTabBar::tab:selected { color: %1; border-bottom-color: %1;"
                  " font-weight: 600; }\n").arg(accent);
    ss += "QTabBar::tab:hover:!selected { color: " + text + "; }\n";
    ss += "QTabWidget::pane { border: none; border-top: 1px solid " + border + "; }\n";

    // ── Status bar ────────────────────────────────────────────────────────────
    ss += QString("QStatusBar { background: %1; border-top: 1px solid %2;"
                  " color: %3; font-size: 11px; padding: 0 8px; }\n")
              .arg(surface, border, textSec);
    ss += "QStatusBar::item { border: none; padding: 0 6px; }\n";

    // ── ListWidget ───────────────────────────────────────────────────────────
    ss += QString("QListWidget { background: %1; border: none; outline: none; }\n").arg(surface);
    ss += QString("QListWidget::item { padding: 8px 12px; border-radius: 6px;"
                  " margin: 1px 6px; color: %1; }\n").arg(textSec);
    ss += QString("QListWidget::item:hover { background: %1; color: %2; }\n").arg(surface3, text);
    ss += QString("QListWidget::item:selected { background: %1; color: %2;"
                  " font-weight: 600; }\n").arg(accentL, accent);

    // ── Group box ─────────────────────────────────────────────────────────────
    ss += QString("QGroupBox { border: 1px solid %1; border-radius: 8px;"
                  " margin-top: 16px; padding-top: 12px; }\n").arg(border);
    ss += QString("QGroupBox::title { color: %1; font-weight: 600; font-size: 12px;"
                  " subcontrol-origin: margin; left: 12px; }\n").arg(textSec);

    ss += "QDialogButtonBox { button-layout: 2; }\n";

    // ── CheckBox / RadioButton ────────────────────────────────────────────────
    ss += QString("QCheckBox { spacing: 8px; color: %1; }\n").arg(text);
    ss += QString("QCheckBox::indicator { width: 16px; height: 16px; border-radius: 4px;"
                  " border: 1.5px solid %1; background: %2; }\n").arg(borderH, surface2);
    ss += QString("QCheckBox::indicator:checked { background: %1; border-color: %1; }\n").arg(accent);
    ss += QString("QRadioButton { spacing: 8px; color: %1; }\n").arg(text);
    ss += QString("QRadioButton::indicator { width: 16px; height: 16px; border-radius: 8px;"
                  " border: 1.5px solid %1; background: %2; }\n").arg(borderH, surface2);
    ss += QString("QRadioButton::indicator:checked { border-color: %1; background: %1; }\n").arg(accent);

    ss += "QScrollArea { border: none; background: transparent; }\n";
    ss += "QScrollArea > QWidget > QWidget { background: transparent; }\n";

    // ── Menu ──────────────────────────────────────────────────────────────────
    ss += QString("QMenu { background: %1; border: 1px solid %2; border-radius: 8px;"
                  " padding: 6px; }\n").arg(surface, border);
    ss += QString("QMenu::item { padding: 7px 16px; border-radius: 5px; color: %1; }\n").arg(text);
    ss += QString("QMenu::item:selected { background: %1; color: %2; }\n").arg(accentL, accent);
    ss += "QMenu::separator { height: 1px; background: " + border + "; margin: 4px 8px; }\n";

    // ── ToolTip ───────────────────────────────────────────────────────────────
    ss += QString("QToolTip { background: %1; color: %2; border: 1px solid %3;"
                  " border-radius: 6px; padding: 5px 9px; font-size: 12px; }\n")
              .arg(surface2, text, borderH);

    return ss;
}
