#include "storage/BackupService.h"
#include <QDir>
#include <QFile>
#include <QDateTime>

BackupService::BackupService(const QString& dataDir, QObject* parent)
    : QObject(parent), m_dataDir(dataDir) {}

void BackupService::onAboutToQuit() { backup(); }

void BackupService::backup()
{
    const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    const QString dest  = m_dataDir + "/backups/" + stamp;
    if (!QDir().mkpath(dest)) return;

    const QStringList files = QDir(m_dataDir).entryList({"*.dat"}, QDir::Files);
    for (const QString& f : files)
        QFile::copy(m_dataDir + "/" + f, dest + "/" + f);

    pruneOldBackups();
}

void BackupService::pruneOldBackups(int keep)
{
    QDir backupDir(m_dataDir + "/backups");
    if (!backupDir.exists()) return;

    QStringList dirs = backupDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    while (dirs.size() > keep) {
        QDir(backupDir.absoluteFilePath(dirs.first())).removeRecursively();
        dirs.removeFirst();
    }
}
