#include <QApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QMessageBox>
#include <QStandardPaths>
#include "app/MainWindow.h"
#include "theme/ThemeManager.h"
#include "storage/StorageService.h"
#include "storage/MigrationV1.h"
#include "storage/BackupService.h"

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

    // One-shot v0→v1 migration: re-encodes double money fields as int64_t cents
    // and localized date strings as ISO "YYYY-MM-DD". Backs up .dat files first.
    // Only runs on first launch after the upgrade; writes a sentinel on success.
    MigrationV1::runIfNeeded(dataPath.toStdString());

    if (!StorageService::instance().initialize(dataPath.toStdString())) {
        const QString reason = QString::fromStdString(
            StorageService::instance().lastInitError());
        QMessageBox::critical(nullptr, "AccountingPro — Cannot Start",
            "Failed to open the data folder.\n\n"
            + (reason.isEmpty() ? QString() : reason + "\n\n")
            + "Path: " + dataPath + "\n\n"
            "Ensure no other instance of AccountingPro is running "
            "and that the folder is writable.");
        return 1;
    }

    // Backup .dat files on clean exit (keeps last 14 snapshots)
    const QString dataPathQ = QString::fromStdString(StorageService::instance().dataDir());
    auto* backupSvc = new BackupService(dataPathQ, &app);
    QObject::connect(&app, &QApplication::aboutToQuit, backupSvc, &BackupService::onAboutToQuit);

    MainWindow window;
    window.show();

    return app.exec();
}
