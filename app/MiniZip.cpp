#include "MiniZip.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>

namespace {
quint32 crc32(const QByteArray& d)
{
    static quint32 table[256];
    static bool init = false;
    if (!init) {
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        init = true;
    }
    quint32 c = 0xFFFFFFFFu;
    for (unsigned char b : d) c = table[(c ^ b) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}
void put16(QByteArray& b, quint16 v) { b.append(char(v & 0xFF)); b.append(char((v >> 8) & 0xFF)); }
void put32(QByteArray& b, quint32 v) { for (int i = 0; i < 4; ++i) b.append(char((v >> (8*i)) & 0xFF)); }
} // namespace

bool MiniZip::add(const QString& nameInZip, const QByteArray& data)
{
    entries_.push_back({ nameInZip, data, crc32(data), 0 });
    return true;
}

bool MiniZip::writeTo(const QString& zipPath) const
{
    QDir().mkpath(QFileInfo(zipPath).absolutePath());
    QFile f(zipPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    QByteArray out;
    QList<Entry> es = entries_;
    // Local file headers + stored (uncompressed) data.
    for (Entry& e : es) {
        e.offset = static_cast<quint32>(out.size());
        const QByteArray name = e.name.toUtf8();
        put32(out, 0x04034b50);          // local file header signature
        put16(out, 20);                  // version needed
        put16(out, 0);                   // flags
        put16(out, 0);                   // method: 0 = store
        put16(out, 0); put16(out, 0);    // mod time / date (fixed => deterministic)
        put32(out, e.crc);
        put32(out, static_cast<quint32>(e.data.size()));   // compressed size
        put32(out, static_cast<quint32>(e.data.size()));   // uncompressed size
        put16(out, static_cast<quint16>(name.size()));
        put16(out, 0);                   // extra len
        out.append(name);
        out.append(e.data);
    }
    // Central directory.
    const quint32 cdStart = static_cast<quint32>(out.size());
    for (const Entry& e : es) {
        const QByteArray name = e.name.toUtf8();
        put32(out, 0x02014b50);          // central dir header signature
        put16(out, 20); put16(out, 20);  // version made by / needed
        put16(out, 0); put16(out, 0);    // flags / method
        put16(out, 0); put16(out, 0);    // time / date
        put32(out, e.crc);
        put32(out, static_cast<quint32>(e.data.size()));
        put32(out, static_cast<quint32>(e.data.size()));
        put16(out, static_cast<quint16>(name.size()));
        put16(out, 0); put16(out, 0);    // extra / comment len
        put16(out, 0); put16(out, 0);    // disk # / internal attrs
        put32(out, 0);                   // external attrs
        put32(out, e.offset);
        out.append(name);
    }
    const quint32 cdSize = static_cast<quint32>(out.size()) - cdStart;
    // End of central directory.
    put32(out, 0x06054b50);
    put16(out, 0); put16(out, 0);
    put16(out, static_cast<quint16>(es.size()));
    put16(out, static_cast<quint16>(es.size()));
    put32(out, cdSize);
    put32(out, cdStart);
    put16(out, 0);                       // comment len

    const bool ok = f.write(out) == out.size();
    f.close();
    return ok;
}
