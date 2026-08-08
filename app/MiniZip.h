#ifndef APP_MINIZIP_H
#define APP_MINIZIP_H

#include <QByteArray>
#include <QString>
#include <QList>

// Minimal store-only (no-compression) ZIP writer — just enough to bundle a crash report into a
// single, universally-openable CrashReport.zip with zero third-party dependencies. Deterministic
// (fixed DOS timestamp), so identical inputs produce byte-identical archives. Not on any
// accounting path.
class MiniZip
{
public:
    bool add(const QString& nameInZip, const QByteArray& data);   // buffer an entry
    bool writeTo(const QString& zipPath) const;                    // emit the .zip

private:
    struct Entry { QString name; QByteArray data; quint32 crc; quint32 offset; };
    QList<Entry> entries_;
};

#endif // APP_MINIZIP_H
