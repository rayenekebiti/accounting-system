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
    return "#1A6FE0";
}

QString ThemeManager::buildStyleSheet(Theme theme) const
{
    const bool dark = (theme == Theme::Dark);

    // ERP enterprise palette — layered dark surfaces, steel-blue accent
    const QString bg       = dark ? "#111318"                 : "#F0F2F6";
    const QString surface  = dark ? "#181B22"                 : "#FFFFFF";
    const QString surface2 = dark ? "#1D2029"                 : "#EAECF2";
    const QString surface3 = dark ? "#232732"                 : "#E0E3EC";
    const QString surface4 = dark ? "#292E3B"                 : "#D5D9E6";
    const QString border   = dark ? "#272D3A"                 : "#C8CDD8";
    const QString borderH  = dark ? "#353D4E"                 : "#A5ADBF";
    const QString text     = dark ? "#C4CBD8"                 : "#1A1E2A";
    const QString textSec  = dark ? "#6B7485"                 : "#58637A";
    const QString textMute = dark ? "#424A57"                 : "#8A90A8";
    const QString accent   = "#1A6FE0";
    const QString accentH  = "#155DC0";
    const QString accentL  = dark ? "rgba(26,111,224,0.13)"  : "rgba(26,111,224,0.09)";
    const QString navBg    = dark ? "#0D0F13"                 : "#1A1E2A";
    const QString success  = "#268C5A";
    const QString warning  = "#BA7B2A";
    const QString danger   = "#C83434";

    QString ss;

    ss += "* { font-family: 'Segoe UI', system-ui, sans-serif;"
          " font-size: 13px; outline: none; }\n";

    ss += QString("QWidget     { background: %1; color: %2; }\n").arg(bg, text);
    ss += QString("QMainWindow { background: %1; }\n").arg(bg);
    ss += QString("QDialog     { background: %1; }\n").arg(surface);

    // ── Toolbar ──────────────────────────────────────────────────────────────
    ss += QString("QToolBar { background: %1; border: none;"
                  " border-bottom: 1px solid %2; spacing: 4px; padding: 3px 12px; }\n")
              .arg(surface, border);
    ss += QString("QToolBar QToolButton { background: transparent; border: 1px solid transparent;"
                  " border-radius: 3px; padding: 4px 10px; color: %1; font-size: 13px; }\n").arg(text);
    ss += QString("QToolBar QToolButton:hover { background: %1; border-color: %2; }\n")
              .arg(surface3, border);
    ss += QString("QToolBar QToolButton:pressed { background: %1; }\n").arg(surface4);
    ss += QString("QToolBar::separator { background: %1; width: 1px; margin: 4px 6px; }\n").arg(border);

    // ── Inputs ───────────────────────────────────────────────────────────────
    const QString inputBase = QString(
        "background: %1; border: 1px solid %2; border-radius: 3px;"
        " padding: 5px 10px; color: %3; selection-background-color: %4;")
            .arg(surface2, border, text, accentL);

    ss += QString("QLineEdit { %1 }\n").arg(inputBase);
    ss += QString("QLineEdit:focus { border-color: %1; }\n").arg(accent);
    ss += QString("QLineEdit:hover:!focus { border-color: %1; }\n").arg(borderH);

    ss += QString("QComboBox { %1 min-width: 80px; }\n").arg(inputBase);
    ss += QString("QComboBox:focus { border-color: %1; }\n").arg(accent);
    ss += QString("QComboBox:hover:!focus { border-color: %1; }\n").arg(borderH);
    ss += "QComboBox::drop-down { border: none; width: 20px; subcontrol-position: right center; }\n";
    ss += "QComboBox::down-arrow { width: 10px; height: 10px; }\n";
    ss += QString("QComboBox QAbstractItemView { background: %1; border: 1px solid %2;"
                  " border-radius: 3px; selection-background-color: %3;"
                  " selection-color: %4; outline: none; padding: 2px; }\n")
              .arg(surface, borderH, accentL, text);

    ss += QString("QSpinBox, QDoubleSpinBox { %1 }\n").arg(inputBase);
    ss += QString("QSpinBox:focus, QDoubleSpinBox:focus { border-color: %1; }\n").arg(accent);
    ss += "QSpinBox::up-button, QSpinBox::down-button,"
          " QDoubleSpinBox::up-button, QDoubleSpinBox::down-button { border: none; width: 16px; }\n";

    ss += QString("QDateEdit { %1 }\n").arg(inputBase);
    ss += QString("QDateEdit:focus { border-color: %1; }\n").arg(accent);
    ss += "QDateEdit::drop-down { border: none; width: 20px; }\n";

    ss += QString("QPlainTextEdit { %1 padding: 8px; }\n").arg(inputBase);
    ss += QString("QPlainTextEdit:focus { border-color: %1; }\n").arg(accent);

    // ── Buttons ──────────────────────────────────────────────────────────────
    ss += QString("QPushButton { background: %1; color: white; border: none;"
                  " border-radius: 3px; padding: 5px 14px; font-weight: 600;"
                  " font-size: 13px; min-height: 28px; }\n").arg(accent);
    ss += QString("QPushButton:hover   { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton:pressed { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton:disabled{ background: %1; color: %2; }\n").arg(border, textSec);

    ss += QString("QPushButton#secondary { background: transparent; color: %1;"
                  " border: 1px solid %2; }\n").arg(text, border);
    ss += QString("QPushButton#secondary:hover { background: %1; border-color: %2; }\n")
              .arg(surface3, borderH);

    ss += QString("QPushButton#danger { background: %1; color: white; border: none; }\n").arg(danger);
    ss += "QPushButton#danger:hover { background: #AA2C2C; }\n";

    ss += QString("QPushButton#flat { background: transparent; color: %1; border: none; min-height: 0; }\n").arg(textSec);
    ss += QString("QPushButton#flat:hover { color: %1; background: %2; }\n").arg(text, surface3);

    // ── Tables ───────────────────────────────────────────────────────────────
    ss += QString("QTableView { background: %1; border: none;"
                  " gridline-color: %2;"
                  " alternate-background-color: %3;"
                  " selection-background-color: %4; selection-color: %5; }\n")
              .arg(surface, border, surface2, accentL, text);
    ss += "QTableView::item { padding: 0px 10px; border: none; }\n";
    ss += QString("QTableView::item:selected { background: %1; color: %2; }\n").arg(accentL, text);
    ss += QString("QTableView::item:hover:!selected { background: %1; }\n").arg(surface3);

    // Header — distinct background, blue bottom accent line, column separators
    ss += QString("QHeaderView { background: %1; border: none; }\n").arg(surface2);
    ss += QString("QHeaderView::section { background: %1; border: none;"
                  " border-right: 1px solid %2; border-bottom: 2px solid %3;"
                  " padding: 0px 10px; height: 30px;"
                  " font-size: 10px; font-weight: 700; color: %4;"
                  " letter-spacing: 0.7px; }\n")
              .arg(surface2, border, accent, textSec);
    ss += "QHeaderView::section:last { border-right: none; }\n";
    // Suppress Qt's built-in sort arrows — avoids the "^" doubling artifact
    ss += "QHeaderView::up-arrow, QHeaderView::down-arrow { image: none; width: 0; height: 0; }\n";

    // ── Scrollbars ───────────────────────────────────────────────────────────
    ss += "QScrollBar:vertical { width: 6px; background: transparent; margin: 0; border: none; }\n";
    ss += QString("QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 30px; }\n").arg(surface4);
    ss += QString("QScrollBar::handle:vertical:hover { background: %1; }\n").arg(textMute);
    ss += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n";
    ss += "QScrollBar:horizontal { height: 6px; background: transparent; margin: 0; border: none; }\n";
    ss += QString("QScrollBar::handle:horizontal { background: %1; border-radius: 3px; min-width: 30px; }\n").arg(surface4);
    ss += QString("QScrollBar::handle:horizontal:hover { background: %1; }\n").arg(textMute);
    ss += "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n";

    // ── Sidebar ───────────────────────────────────────────────────────────────
    ss += QString("QWidget#navPanel { background: %1; }\n").arg(navBg);

    // ── Page header ──────────────────────────────────────────────────────────
    ss += QString("QWidget#pageHeaderWidget { background: %1;"
                  " border-bottom: 1px solid %2; }\n").arg(surface, border);

    // ── Labels ───────────────────────────────────────────────────────────────
    ss += "QLabel { background: transparent; }\n";
    ss += QString("QLabel#muted       { color: %1; font-size: 11px; }\n").arg(textSec);
    ss += QString("QLabel#pageTitle   { font-size: 15px; font-weight: 700; color: %1; }\n").arg(text);
    ss += QString("QLabel#sectionTitle { font-size: 10px; font-weight: 700;"
                  " color: %1; letter-spacing: 0.6px; }\n").arg(textSec);
    ss += QString("QLabel#kpiTitle  { font-size: 10px; font-weight: 600;"
                  " color: %1; letter-spacing: 0.5px; }\n").arg(textSec);
    ss += QString("QLabel#kpiValue  { font-size: 22px; font-weight: 700; color: %1; }\n").arg(text);

    // ── Cards / Frames ───────────────────────────────────────────────────────
    ss += QString("QFrame#card { background: %1; border: 1px solid %2;"
                  " border-radius: 3px; }\n").arg(surface, border);
    ss += QString("QFrame#dangerCard { background: %1; border: 1px solid %2;"
                  " border-left: 3px solid %3; border-radius: 3px; }\n")
              .arg(surface, border, danger);
    ss += "QFrame#kpiCard { background: transparent; border: none; }\n";

    // ── Splitter ──────────────────────────────────────────────────────────────
    ss += QString("QSplitter::handle { background: %1; }\n").arg(border);
    ss += "QSplitter::handle:horizontal { width: 1px; }\n";
    ss += "QSplitter::handle:vertical   { height: 1px; }\n";

    // ── Tabs ──────────────────────────────────────────────────────────────────
    ss += "QTabBar { background: transparent; border: none; }\n";
    ss += QString("QTabBar::tab { background: transparent; border: none;"
                  " border-bottom: 2px solid transparent; padding: 7px 16px;"
                  " margin-right: 2px; font-size: 13px; color: %1; }\n").arg(textSec);
    ss += QString("QTabBar::tab:selected { color: %1; border-bottom-color: %1;"
                  " font-weight: 600; }\n").arg(accent);
    ss += "QTabBar::tab:hover:!selected { color: " + text + "; background: " + surface3 + "; border-radius: 3px 3px 0 0; }\n";
    ss += "QTabWidget::pane { border: none; border-top: 1px solid " + border + "; }\n";

    // ── Status bar ────────────────────────────────────────────────────────────
    ss += QString("QStatusBar { background: %1; border-top: 1px solid %2;"
                  " color: %3; font-size: 11px; padding: 0 8px; }\n")
              .arg(surface, border, textSec);
    ss += "QStatusBar::item { border: none; padding: 0 6px; }\n";

    // ── ListWidget ───────────────────────────────────────────────────────────
    ss += QString("QListWidget { background: %1; border: none; outline: none; }\n").arg(surface2);
    ss += QString("QListWidget::item { padding: 7px 14px; border-radius: 3px;"
                  " margin: 1px 4px; color: %1; }\n").arg(textSec);
    ss += QString("QListWidget::item:hover { background: %1; color: %2; }\n").arg(surface3, text);
    ss += QString("QListWidget::item:selected { background: %1; color: %2;"
                  " font-weight: 600; }\n").arg(accentL, accent);

    // ── Group box ─────────────────────────────────────────────────────────────
    ss += QString("QGroupBox { border: 1px solid %1; border-radius: 3px;"
                  " margin-top: 14px; padding-top: 10px; }\n").arg(border);
    ss += QString("QGroupBox::title { color: %1; font-weight: 700; font-size: 10px;"
                  " subcontrol-origin: margin; left: 10px; letter-spacing: 0.5px; }\n").arg(textSec);

    ss += "QDialogButtonBox { button-layout: 2; }\n";

    // ── CheckBox / RadioButton ────────────────────────────────────────────────
    ss += QString("QCheckBox { spacing: 8px; color: %1; }\n").arg(text);
    ss += QString("QCheckBox::indicator { width: 14px; height: 14px; border-radius: 3px;"
                  " border: 1.5px solid %1; background: %2; }\n").arg(border, surface2);
    ss += QString("QCheckBox::indicator:checked { background: %1; border-color: %1; }\n").arg(accent);
    ss += QString("QRadioButton { spacing: 8px; color: %1; }\n").arg(text);
    ss += QString("QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px;"
                  " border: 1.5px solid %1; background: %2; }\n").arg(border, surface2);
    ss += QString("QRadioButton::indicator:checked { border-color: %1; background: %1; }\n").arg(accent);

    ss += "QScrollArea { border: none; background: transparent; }\n";
    ss += "QScrollArea > QWidget > QWidget { background: transparent; }\n";

    // ── Menu ──────────────────────────────────────────────────────────────────
    ss += QString("QMenu { background: %1; border: 1px solid %2; border-radius: 4px;"
                  " padding: 4px; }\n").arg(surface, borderH);
    ss += QString("QMenu::item { padding: 6px 14px; border-radius: 3px; color: %1; }\n").arg(text);
    ss += QString("QMenu::item:selected { background: %1; color: %2; }\n").arg(accentL, accent);
    ss += "QMenu::separator { height: 1px; background: " + border + "; margin: 3px 6px; }\n";

    // ── ToolTip ───────────────────────────────────────────────────────────────
    ss += QString("QToolTip { background: %1; color: %2; border: 1px solid %3;"
                  " border-radius: 4px; padding: 4px 8px; font-size: 12px; }\n")
              .arg(surface2, text, borderH);

    return ss;
}
