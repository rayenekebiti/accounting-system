#pragma once
#include <algorithm>   // std::min
// One-shot migration: converts v0 .dat files to v1 format.
//
// v0 format (what was written before Phase 1):
//   - Money fields: IEEE 754 double (dollars.cents)
//   - Date fields:  "d MMM yyyy" C-locale string (e.g. "5 Jun 2026") in a 12-byte slot
//
// v1 format (after Phase 1):
//   - Money fields: int64_t cents (same 8-byte slot, different encoding)
//   - Date fields:  "YYYY-MM-DD\0\0" ISO string (same 12-byte slot, different encoding)
//
// Record sizes are unchanged — only the byte-level encoding of money and date fields differs.
// A sentinel file migration_v1.done in the data directory prevents double-runs.
//
// Call MigrationV1::runIfNeeded(dataDir) before StorageService::initialize().

#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <vector>
#include <QString>
#include <QDate>
#include <QLocale>
#include <QFile>
#include <QFileInfo>

class MigrationV1 {
public:
    // Returns true if everything is OK (already migrated, or migration succeeded).
    // Returns false only on a hard I/O failure — the caller should warn but keep going.
    static bool runIfNeeded(const std::string& dataDir)
    {
        const QString sentinel = QString::fromStdString(dataDir) + "/migration_v1.done";
        if (QFile::exists(sentinel))
            return true;

        struct Spec {
            const char*      name;
            std::size_t      recSize;
            std::vector<int> moneyOffsets;   // 8-byte int64_t fields (were double)
            std::vector<int> dateOffsets;    // 12-byte date string slots
        };

        const Spec specs[] = {
            { "customers.dat",  128, {114},       {}     },
            { "invoices.dat",    96, {44, 52, 60}, {20, 32} },
            { "payments.dat",    64, {38},         {26}   },
            { "suppliers.dat",  128, {114},        {}     },
            { "products.dat",   192, {146, 154},   {}     },
        };

        for (const auto& s : specs) {
            const std::string path = dataDir + "/" + s.name;
            if (!QFile::exists(QString::fromStdString(path)))
                continue;
            if (!migrateFile(path, s.recSize, s.moneyOffsets, s.dateOffsets))
                return false;
        }

        QFile f(sentinel);
        if (!f.open(QIODevice::WriteOnly))
            return false;
        f.write("v1");
        return true;
    }

private:
    static bool migrateFile(const std::string& path, std::size_t recSize,
                             const std::vector<int>& moneyOffsets,
                             const std::vector<int>& dateOffsets)
    {
        // Back up the file before touching it.
        QFile::copy(QString::fromStdString(path),
                    QString::fromStdString(path + ".bak"));

        QFile f(QString::fromStdString(path));
        if (!f.open(QIODevice::ReadWrite))
            return false;

        const qint64 fileSize = f.size();
        if (fileSize <= 0 || static_cast<std::size_t>(fileSize) % recSize != 0) {
            f.close();
            return true;  // empty or corrupt — leave it alone
        }

        QByteArray data = f.readAll();
        char* buf = data.data();
        const std::size_t count = static_cast<std::size_t>(fileSize) / recSize;

        for (std::size_t i = 0; i < count; ++i) {
            char* rec = buf + i * recSize;

            for (int off : moneyOffsets) {
                double dval;
                std::memcpy(&dval, rec + off, sizeof(dval));
                // Round half-away-from-zero: $12.345 → 1235 cents.
                double raw = dval * 100.0;
                std::int64_t cents = static_cast<std::int64_t>(
                    raw >= 0.0 ? std::floor(raw + 0.5) : std::ceil(raw - 0.5));
                std::memcpy(rec + off, &cents, sizeof(cents));
            }

            for (int off : dateOffsets) {
                char tmp[13] = {};
                std::memcpy(tmp, rec + off, 12);
                // Try to parse old "d MMM yyyy" format; skip if already ISO or blank.
                QDate d = QLocale::c().toDate(QString::fromLatin1(tmp), "d MMM yyyy");
                if (d.isValid()) {
                    const QByteArray iso = d.toString("yyyy-MM-dd").toLatin1();
                    std::memset(rec + off, 0, 12);
                    const int len = std::min(iso.size(), static_cast<qsizetype>(10));
                    std::memcpy(rec + off, iso.constData(), static_cast<std::size_t>(len));
                }
            }
        }

        f.seek(0);
        f.write(data);
        f.flush();
        return true;
    }
};
