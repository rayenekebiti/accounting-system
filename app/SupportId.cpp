#include "SupportId.h"

#include <QSettings>
#include <QUuid>

namespace supportid {

QString get()
{
    QSettings s;
    QString id = s.value(QStringLiteral("support/id")).toString();
    if (id.isEmpty()) {
        // 4 random bytes (32 bits) from a v4 UUID → 8 uppercase hex chars, grouped for readability.
        // Random, not derived from anything identifying — see the header note.
        const QByteArray hex = QUuid::createUuid().toRfc4122().left(4).toHex().toUpper();
        id = QStringLiteral("OCC-") + QString::fromLatin1(hex.left(4))
             + QLatin1Char('-')     + QString::fromLatin1(hex.mid(4, 4));
        s.setValue(QStringLiteral("support/id"), id);
        s.sync();
    }
    return id;
}

} // namespace supportid
