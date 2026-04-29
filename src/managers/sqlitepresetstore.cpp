#include "sqlitepresetstore.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

SqlitePresetStore::SqlitePresetStore(const QString &category, const QString &dbFileName)
    : m_category(category)
    , m_connectionName(QStringLiteral("SqlitePresetStore_") + category + QLatin1Char('_')
                       + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QString err;
    m_open = initDatabase(dbFileName, err);
}

SqlitePresetStore::~SqlitePresetStore()
{
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool SqlitePresetStore::initDatabase(const QString &dbFileName, QString &errorMsg)
{
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(dataDir)) {
        errorMsg = QCoreApplication::translate("SqlitePresetStore",
                                               "Cannot create data directory: %1").arg(dataDir);
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dataDir + QLatin1Char('/') + dbFileName);

    if (!m_db.open()) {
        errorMsg = m_db.lastError().text();
        return false;
    }

    return createSchema(errorMsg);
}

bool SqlitePresetStore::createSchema(QString &errorMsg)
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS presets ("
            "  category   TEXT NOT NULL,"
            "  name       TEXT NOT NULL,"
            "  payload    BLOB NOT NULL,"
            "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "  UNIQUE(category, name)"
            ")"))) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

QStringList SqlitePresetStore::listPresets() const
{
    QStringList names;
    if (!m_open) return names;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT name FROM presets WHERE category = :cat ORDER BY name ASC"));
    q.bindValue(QStringLiteral(":cat"), m_category);
    if (!q.exec()) return names;

    while (q.next())
        names.append(q.value(0).toString());
    return names;
}

bool SqlitePresetStore::savePreset(const QString &name,
                                   const QByteArray &payload,
                                   QString &errorMsg)
{
    if (!m_open) {
        errorMsg = QCoreApplication::translate("SqlitePresetStore", "Database is not open.");
        return false;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        errorMsg = QCoreApplication::translate("SqlitePresetStore", "Preset name must not be empty.");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO presets (category, name, payload, updated_at) "
        "VALUES (:cat, :name, :payload, CURRENT_TIMESTAMP) "
        "ON CONFLICT(category, name) DO UPDATE SET "
        "  payload    = excluded.payload,"
        "  updated_at = CURRENT_TIMESTAMP"));
    q.bindValue(QStringLiteral(":cat"),     m_category);
    q.bindValue(QStringLiteral(":name"),    trimmed);
    q.bindValue(QStringLiteral(":payload"), payload);

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}

QByteArray SqlitePresetStore::loadPreset(const QString &name) const
{
    if (!m_open) return {};

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT payload FROM presets WHERE category = :cat AND name = :name"));
    q.bindValue(QStringLiteral(":cat"),  m_category);
    q.bindValue(QStringLiteral(":name"), name.trimmed());
    if (!q.exec() || !q.next())
        return {};
    return q.value(0).toByteArray();
}

bool SqlitePresetStore::deletePreset(const QString &name, QString &errorMsg)
{
    if (!m_open) {
        errorMsg = QCoreApplication::translate("SqlitePresetStore", "Database is not open.");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM presets WHERE category = :cat AND name = :name"));
    q.bindValue(QStringLiteral(":cat"),  m_category);
    q.bindValue(QStringLiteral(":name"), name.trimmed());
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }
    return true;
}
