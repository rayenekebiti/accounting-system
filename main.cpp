#include <QApplication>
#include "app/MainWindow.h"
#include "theme/ThemeManager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("AccountingPro");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("AccountingPro");

    ThemeManager::instance().apply(app, ThemeManager::Theme::Light);

    MainWindow window;
    window.show();

    return app.exec();
}
