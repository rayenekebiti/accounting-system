#pragma once
#include <QObject>
#include <QString>

class QApplication;

class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Theme { Light, Dark };

    static ThemeManager& instance();

    void apply(QApplication& app, Theme theme = Theme::Light);
    void setTheme(Theme theme);
    Theme currentTheme() const { return m_theme; }

    QString accentColor() const;

signals:
    void themeChanged(Theme theme);

private:
    explicit ThemeManager(QObject* parent = nullptr);
    QString buildStyleSheet(Theme theme) const;

    Theme m_theme = Theme::Light;
};
