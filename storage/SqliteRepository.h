#pragma once
// Phase 6 — SQLite backend (blob-per-row pattern).
// Enabled via CMake option: -DACCT_USE_SQLITE=ON
//
// Each entity is stored as a fixed-size BLOB using the same serialize/deserialize
// format as BinaryRecordFile, so no schema migration is needed — just swap the
// backend.  Uses Qt6::Sql (bundled with Qt 6; no external sqlite3 amalgamation).
//
// Usage:
//   using SqliteCustomerRepo = SqliteRepository<Customer,
//       CUSTOMER_RECORD_SIZE, CUSTOMER_DELETED_OFFSET>;
//
#ifdef ACCT_USE_SQLITE
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QByteArray>
#include <QString>
#include <vector>
#include <stdexcept>
#include <cstring>

template<typename Entity, std::size_t RecordSize, std::size_t DeletedOffset>
class SqliteRepository {
public:
    explicit SqliteRepository(const QString& dbPath, const QString& table)
        : m_table(table)
    {
        const QString connName = "acct_" + table;
        if (QSqlDatabase::contains(connName))
            m_db = QSqlDatabase::database(connName);
        else
            m_db = QSqlDatabase::addDatabase("QSQLITE", connName);

        m_db.setDatabaseName(dbPath);
        if (!m_db.open())
            throw std::runtime_error(("Cannot open SQLite DB: " + m_db.lastError().text()).toStdString());

        QSqlQuery q(m_db);
        if (!q.exec(QString("CREATE TABLE IF NOT EXISTS %1 "
                            "(id INTEGER PRIMARY KEY, data BLOB NOT NULL)").arg(table)))
            throw std::runtime_error(("Cannot create table " + table + ": "
                                      + q.lastError().text()).toStdString());
    }

    void save(Entity& entity)
    {
        char buf[RecordSize]; std::memset(buf, 0, RecordSize);
        entity.serialize(buf);

        QSqlQuery q(m_db);
        q.prepare(QString("INSERT OR REPLACE INTO %1 (id, data) VALUES (?, ?)").arg(m_table));
        q.addBindValue(static_cast<qint64>(entity.getId()));
        q.addBindValue(QByteArray(buf, RecordSize));
        if (!q.exec())
            throw std::runtime_error(q.lastError().text().toStdString());
    }

    void update(const Entity& entity) { const_cast<Entity&>(entity); save(const_cast<Entity&>(entity)); }

    void remove(uint32_t id)
    {
        QSqlQuery q(m_db);
        q.prepare(QString("SELECT data FROM %1 WHERE id=?").arg(m_table));
        q.addBindValue(static_cast<qint64>(id));
        if (!q.exec() || !q.next()) return;

        QByteArray buf = q.value(0).toByteArray();
        if (buf.size() < static_cast<int>(RecordSize)) return;
        buf[DeletedOffset] = 1;

        QSqlQuery u(m_db);
        u.prepare(QString("UPDATE %1 SET data=? WHERE id=?").arg(m_table));
        u.addBindValue(buf);
        u.addBindValue(static_cast<qint64>(id));
        if (!u.exec())
            throw std::runtime_error(u.lastError().text().toStdString());
    }

    Entity find(uint32_t id)
    {
        QSqlQuery q(m_db);
        q.prepare(QString("SELECT data FROM %1 WHERE id=?").arg(m_table));
        q.addBindValue(static_cast<qint64>(id));
        if (!q.exec() || !q.next())
            throw std::runtime_error("Record not found");
        const QByteArray buf = q.value(0).toByteArray();
        Entity e;
        e.deserialize(buf.constData());
        return e;
    }

    std::vector<Entity> loadAll()
    {
        QSqlQuery q(m_db);
        if (!q.exec(QString("SELECT data FROM %1").arg(m_table)))
            throw std::runtime_error(q.lastError().text().toStdString());

        std::vector<Entity> result;
        while (q.next()) {
            const QByteArray buf = q.value(0).toByteArray();
            if (buf.size() < static_cast<int>(RecordSize)) continue;
            if (static_cast<unsigned char>(buf[DeletedOffset])) continue;
            Entity e; e.deserialize(buf.constData());
            result.push_back(e);
        }
        return result;
    }

private:
    QSqlDatabase m_db;
    QString      m_table;
};

// Convenience aliases — add more entity types as needed.
#include "Customer.h"
#include "Supplier.h"
#include "Invoice.h"
#include "Payment.h"

using SqliteCustomerRepository = SqliteRepository<Customer, CUSTOMER_RECORD_SIZE, CUSTOMER_DELETED_OFFSET>;
using SqliteInvoiceRepository  = SqliteRepository<Invoice,  INVOICE_RECORD_SIZE,  INVOICE_DELETED_OFFSET>;

#endif // ACCT_USE_SQLITE
