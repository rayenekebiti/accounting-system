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
    return m_theme == Theme::Dark ? "#42A5F5" : "#1565C0";
}

QString ThemeManager::buildStyleSheet(Theme theme) const
{
    const bool dark = (theme == Theme::Dark);
    const QString bg        = dark ? "#1E1E1E" : "#F5F5F5";
    const QString surface   = dark ? "#2D2D2D" : "#FFFFFF";
    const QString border    = dark ? "#3A3A3A" : "#E0E0E0";
    const QString text      = dark ? "#E0E0E0" : "#212121";
    const QString muted     = dark ? "#9E9E9E" : "#757575";
    const QString accent    = dark ? "#42A5F5" : "#1565C0";
    const QString accentH   = dark ? "#1E88E5" : "#0D47A1";
    const QString navBg     = dark ? "#1A1A1A" : "#263238";
    const QString navText   = "#B0BEC5";
    const QString navSelBg  = dark ? "#2D2D2D" : "#37474F";

    QString ss;

    ss += QString("* { font-family: 'Segoe UI', system-ui, sans-serif; font-size: 13px; }\n");

    ss += QString("QWidget { background: %1; color: %2; }\n").arg(bg, text);
    ss += QString("QMainWindow { background: %1; }\n").arg(bg);

    // Toolbar
    ss += QString("QToolBar { background: %1; border-bottom: 1px solid %2; spacing: 8px; padding: 4px 8px; }\n")
              .arg(surface, border);
    ss += QString("QToolBar QToolButton { background: transparent; border: 1px solid transparent; "
                  "border-radius: 4px; padding: 4px 10px; color: %1; }\n").arg(text);
    ss += QString("QToolBar QToolButton:hover { background: %1; }\n").arg(border);

    // Inputs
    ss += QString("QLineEdit { background: %1; border: 1px solid %2; border-radius: 4px; "
                  "padding: 4px 8px; color: %3; }\n").arg(surface, border, text);
    ss += QString("QLineEdit:focus { border-color: %1; }\n").arg(accent);

    ss += QString("QComboBox { background: %1; border: 1px solid %2; border-radius: 4px; "
                  "padding: 4px 8px; color: %3; }\n").arg(surface, border, text);
    ss += QString("QComboBox:hover { border-color: %1; }\n").arg(accent);
    ss += "QComboBox::drop-down { border: none; width: 20px; }\n";

    // Buttons
    ss += QString("QPushButton { background: %1; color: white; border: none; "
                  "border-radius: 4px; padding: 6px 16px; font-weight: 500; }\n").arg(accent);
    ss += QString("QPushButton:hover { background: %1; }\n").arg(accentH);
    ss += QString("QPushButton#secondary { background: transparent; color: %1; border: 1px solid %1; }\n").arg(accent);
    ss += QString("QPushButton#secondary:hover { background: rgba(21,101,192,0.08); }\n");
    ss += QString("QPushButton#danger { background: #C62828; }\n");
    ss += QString("QPushButton#danger:hover { background: #B71C1C; }\n");
    ss += QString("QPushButton:disabled { background: %1; color: %2; }\n").arg(border, muted);

    // Table
    ss += QString("QTableView { background: %1; border: 1px solid %2; border-radius: 4px; "
                  "gridline-color: %2; alternate-background-color: %3; }\n")
              .arg(surface, border, bg);
    ss += QString("QTableView::item { padding: 6px 8px; border: none; }\n");
    ss += QString("QTableView::item:selected { background: rgba(21,101,192,0.15); color: %1; }\n").arg(text);
    ss += QString("QTableView { selection-background-color: rgba(21,101,192,0.15); selection-color: %1; }\n").arg(text);

    ss += QString("QHeaderView::section { background: %1; border: none; border-bottom: 2px solid %2; "
                  "border-right: 1px solid %2; padding: 6px 8px; font-weight: 600; color: %3; }\n")
              .arg(bg, border, muted);

    // Scrollbars
    ss += "QScrollBar:vertical { width: 8px; background: transparent; margin: 0; }\n";
    ss += QString("QScrollBar::handle:vertical { background: %1; border-radius: 4px; min-height: 20px; }\n").arg(border);
    ss += "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n";
    ss += "QScrollBar:horizontal { height: 8px; background: transparent; margin: 0; }\n";
    ss += QString("QScrollBar::handle:horizontal { background: %1; border-radius: 4px; min-width: 20px; }\n").arg(border);
    ss += "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }\n";

    // Sidebar list
    ss += QString("QListWidget { background: %1; border: none; outline: none; }\n").arg(navBg);
    ss += QString("QListWidget::item { padding: 10px 16px; border-left: 3px solid transparent; "
                  "color: %1; }\n").arg(navText);
    ss += "QListWidget::item:hover { background: rgba(255,255,255,0.08); }\n";
    ss += QString("QListWidget::item:selected { background: %1; color: white; border-left-color: %2; }\n")
              .arg(navSelBg, accent);

    // Status bar
    ss += QString("QStatusBar { background: %1; border-top: 1px solid %2; color: %3; }\n")
              .arg(surface, border, muted);
    ss += "QStatusBar::item { border: none; }\n";

    // Labels
    ss += "QLabel { background: transparent; }\n";
    ss += QString("QLabel#muted { color: %1; }\n").arg(muted);
    ss += QString("QLabel#pageTitle { font-size: 20px; font-weight: 600; color: %1; }\n").arg(text);
    ss += QString("QLabel#sectionTitle { font-size: 14px; font-weight: 600; color: %1; }\n").arg(text);

    // Splitter
    ss += QString("QSplitter::handle { background: %1; }\n").arg(border);
    ss += "QSplitter::handle:horizontal { width: 1px; }\n";
    ss += "QSplitter::handle:vertical { height: 1px; }\n";

    // Dialog
    ss += QString("QDialog { background: %1; }\n").arg(surface);

    // Tabs
    ss += "QTabBar::tab { background: transparent; border: none; "
          "border-bottom: 2px solid transparent; padding: 8px 16px; }\n";
    ss += QString("QTabBar::tab:selected { color: %1; border-bottom-color: %1; font-weight: 600; }\n").arg(accent);
    ss += QString("QTabBar::tab:!selected { color: %1; }\n").arg(muted);
    ss += "QTabBar::tab:hover:!selected { background: rgba(128,128,128,0.08); }\n";
    ss += "QTabWidget::pane { border: none; }\n";

    // Frame / card
    ss += QString("QFrame#card { background: %1; border: 1px solid %2; border-radius: 8px; }\n")
              .arg(surface, border);

    // Group box
    ss += QString("QGroupBox { border: 1px solid %1; border-radius: 4px; margin-top: 12px; padding-top: 8px; }\n")
              .arg(border);
    ss += QString("QGroupBox::title { color: %1; font-weight: 600; subcontrol-origin: margin; left: 8px; }\n")
              .arg(muted);

    // Spin box
    ss += QString("QSpinBox, QDoubleSpinBox { background: %1; border: 1px solid %2; "
                  "border-radius: 4px; padding: 4px 8px; color: %3; }\n").arg(surface, border, text);
    ss += QString("QSpinBox:focus, QDoubleSpinBox:focus { border-color: %1; }\n").arg(accent);
    ss += "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button "
          "{ border: none; width: 16px; }\n";

    // Date edit
    ss += QString("QDateEdit { background: %1; border: 1px solid %2; border-radius: 4px; "
                  "padding: 4px 8px; color: %3; }\n").arg(surface, border, text);
    ss += QString("QDateEdit:focus { border-color: %1; }\n").arg(accent);
    ss += "QDateEdit::drop-down { border: none; width: 20px; }\n";

    // Plain text edit
    ss += QString("QPlainTextEdit { background: %1; border: 1px solid %2; border-radius: 4px; "
                  "padding: 8px; color: %3; }\n").arg(surface, border, text);
    ss += QString("QPlainTextEdit:focus { border-color: %1; }\n").arg(accent);

    // Dialog button box
    ss += "QDialogButtonBox { button-layout: 2; }\n";

    // Navigation panel background widget
    ss += QString("QWidget#navPanel { background: %1; }\n").arg(navBg);
    ss += QString("QWidget#pageHeaderWidget { background: %1; border-bottom: 1px solid %2; }\n")
              .arg(surface, border);

    // CheckBox
    ss += QString("QCheckBox { spacing: 6px; color: %1; }\n").arg(text);

    // RadioButton
    ss += QString("QRadioButton { spacing: 6px; color: %1; }\n").arg(text);

    return ss;
}
