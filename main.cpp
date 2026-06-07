#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QStandardPaths>
#include "app/MainWindow.h"
#include "theme/ThemeManager.h"
#include "storage/StorageService.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QFont appFont = app.font();
    appFont.setFamilies({"Segoe UI", "Noto Color Emoji"});
    app.setFont(appFont);
    app.setApplicationName("AccountingPro");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("AccountingPro");

    ThemeManager::instance().apply(app, ThemeManager::Theme::Dark);

    const QString dataPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    StorageService::instance().initialize(dataPath.toStdString());

    MainWindow window;
    window.show();

    return app.exec();
}
