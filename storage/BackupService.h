#pragma once
#include <QObject>
#include <QString>

class BackupService : public QObject {
    Q_OBJECT
public:
    explicit BackupService(const QString& dataDir, QObject* parent = nullptr);
    void backup();

public slots:
    void onAboutToQuit();

private:
    void pruneOldBackups(int keep = 14);
    QString m_dataDir;
};
