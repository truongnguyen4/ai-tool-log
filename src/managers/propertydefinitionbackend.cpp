#include "propertydefinitionbackend.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

// ─────────────────────────────────────────────────────────────────────────────
// JSON field names (keep in one place so they are easy to version/rename)
// ─────────────────────────────────────────────────────────────────────────────
namespace JsonFields {
    constexpr const char* ROOT        = "propertyDefinitions";
    constexpr const char* ID          = "id";
    constexpr const char* NAME        = "name";
    constexpr const char* SUPPORTED   = "isSupported";
    constexpr const char* VALUE       = "value";
    constexpr const char* DEFAULT     = "defaultValue";
    constexpr const char* NEED_REBOOT = "needReboot";
    constexpr const char* TYPE        = "type";
    constexpr const char* READ_ONLY   = "readOnly";
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

PropertyDefinitionBackend::PropertyDefinitionBackend(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("PropDefBackend_") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    QString errorMsg;
    m_dbOpen = initDatabase(errorMsg);
    // Non-fatal: the app still works; DB-dependent operations will report the error.
}

PropertyDefinitionBackend::~PropertyDefinitionBackend()
{
    if (m_db.isOpen())
        m_db.close();

    // Drop the member reference before calling removeDatabase,
    // otherwise Qt warns that the connection is still in use.
    m_db = QSqlDatabase();

    QSqlDatabase::removeDatabase(m_connectionName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::initDatabase(QString &errorMsg)
{
    const QString dataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    if (!QDir().mkpath(dataDir)) {
        errorMsg = tr("Cannot create data directory: %1").arg(dataDir);
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dataDir + QStringLiteral("/propertydefs.db"));

    if (!m_db.open()) {
        errorMsg = m_db.lastError().text();
        return false;
    }

    return createSchema(errorMsg);
}

bool PropertyDefinitionBackend::createSchema(QString &errorMsg)
{
    QSqlQuery q(m_db);

    // Enable foreign-key enforcement in SQLite (off by default)
    if (!q.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        errorMsg = q.lastError().text();
        return false;
    }

    // Named sets
    const QString createSets = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS property_sets ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT    NOT NULL UNIQUE,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")");

    if (!q.exec(createSets)) {
        errorMsg = q.lastError().text();
        return false;
    }

    // Individual entries belonging to a named set
    const QString createEntries = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS property_set_entries ("
        "  id           INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  set_id       INTEGER NOT NULL REFERENCES property_sets(id) ON DELETE CASCADE,"
        "  prop_id      TEXT    NOT NULL DEFAULT '',"
        "  prop_name    TEXT    NOT NULL,"
        "  is_supported INTEGER NOT NULL DEFAULT 0,"
        "  value        TEXT    NOT NULL DEFAULT '',"
        "  default_value TEXT   NOT NULL DEFAULT '',"
        "  need_reboot  INTEGER NOT NULL DEFAULT 0,"
        "  type         TEXT    NOT NULL DEFAULT '',"
        "  read_only    INTEGER NOT NULL DEFAULT 0"
        ")");

    if (!q.exec(createEntries)) {
        errorMsg = q.lastError().text();
        return false;
    }

    // Migration: add value column for databases created before this field existed
    q.exec(QStringLiteral(
        "ALTER TABLE property_set_entries ADD COLUMN value TEXT NOT NULL DEFAULT ''"));
    // Ignore error – the column already exists in newly-created databases

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionDatabase — save
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::savePropertySet(const QString &setName,
                                                const QVector<PropertyDefinition> &definitions,
                                                QString &errorMsg)
{
    if (!m_dbOpen) {
        errorMsg = tr("Database is not open.");
        return false;
    }

    if (setName.trimmed().isEmpty()) {
        errorMsg = tr("Set name must not be empty.");
        return false;
    }

    if (!m_db.transaction()) {
        errorMsg = m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);

    // Upsert into property_sets
    q.prepare(QStringLiteral(
        "INSERT INTO property_sets (name) VALUES (:name) "
        "ON CONFLICT(name) DO UPDATE SET name = excluded.name"));
    q.bindValue(QStringLiteral(":name"), setName.trimmed());

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        m_db.rollback();
        return false;
    }

    // Retrieve the set id
    q.prepare(QStringLiteral("SELECT id FROM property_sets WHERE name = :name"));
    q.bindValue(QStringLiteral(":name"), setName.trimmed());
    if (!q.exec() || !q.next()) {
        errorMsg = q.lastError().text();
        m_db.rollback();
        return false;
    }
    const qlonglong setId = q.value(0).toLongLong();

    // Remove all existing entries for this set (clean overwrite)
    q.prepare(QStringLiteral("DELETE FROM property_set_entries WHERE set_id = :sid"));
    q.bindValue(QStringLiteral(":sid"), setId);
    if (!q.exec()) {
        errorMsg = q.lastError().text();
        m_db.rollback();
        return false;
    }

    // Insert fresh entries
    q.prepare(QStringLiteral(
        "INSERT INTO property_set_entries "
        "(set_id, prop_id, prop_name, is_supported, value, default_value, need_reboot, type, read_only) "
        "VALUES (:sid, :pid, :pname, :sup, :val, :def, :rb, :type, :ro)"));

    for (const PropertyDefinition &def : definitions) {
        q.bindValue(QStringLiteral(":sid"),   setId);
        q.bindValue(QStringLiteral(":pid"),   def.id);
        q.bindValue(QStringLiteral(":pname"), def.name);
        q.bindValue(QStringLiteral(":sup"),   def.isSupported ? 1 : 0);
        q.bindValue(QStringLiteral(":val"),   def.value);
        q.bindValue(QStringLiteral(":def"),   def.defaultValue);
        q.bindValue(QStringLiteral(":rb"),    def.needReboot ? 1 : 0);
        q.bindValue(QStringLiteral(":type"),  def.type);
        q.bindValue(QStringLiteral(":ro"),    def.readOnly ? 1 : 0);
        if (!q.exec()) {
            errorMsg = q.lastError().text();
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        errorMsg = m_db.lastError().text();
        m_db.rollback();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionDatabase — load
// ─────────────────────────────────────────────────────────────────────────────

QVector<PropertyDefinition> PropertyDefinitionBackend::loadPropertySet(
    const QString &setName, QString &errorMsg)
{
    QVector<PropertyDefinition> result;

    if (!m_dbOpen) {
        errorMsg = tr("Database is not open.");
        return result;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT e.prop_id, e.prop_name, e.is_supported, e.value, e.default_value, "
        "       e.need_reboot, e.type, e.read_only "
        "FROM property_set_entries e "
        "JOIN property_sets s ON s.id = e.set_id "
        "WHERE s.name = :name "
        "ORDER BY e.id"));
    q.bindValue(QStringLiteral(":name"), setName.trimmed());

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return result;
    }

    while (q.next()) {
        PropertyDefinition def;
        def.id           = q.value(0).toString();
        def.name         = q.value(1).toString();
        def.isSupported  = q.value(2).toInt() != 0;
        def.value        = q.value(3).toString();
        def.defaultValue = q.value(4).toString();
        def.needReboot   = q.value(5).toInt() != 0;
        def.type         = q.value(6).toString();
        def.readOnly     = q.value(7).toInt() != 0;
        result.append(def);
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionDatabase — list names
// ─────────────────────────────────────────────────────────────────────────────

QStringList PropertyDefinitionBackend::listPropertySetNames() const
{
    QStringList names;

    if (!m_dbOpen)
        return names;

    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT name FROM property_sets ORDER BY name ASC")))
        return names;

    while (q.next())
        names.append(q.value(0).toString());

    return names;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionDatabase — delete
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::deletePropertySet(const QString &setName, QString &errorMsg)
{
    if (!m_dbOpen) {
        errorMsg = tr("Database is not open.");
        return false;
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM property_sets WHERE name = :name"));
    q.bindValue(QStringLiteral(":name"), setName.trimmed());

    if (!q.exec()) {
        errorMsg = q.lastError().text();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionExporter — export to JSON
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::exportToFile(const QString &filePath,
                                             const QVector<PropertyDefinition> &definitions,
                                             QString &errorMsg)
{
    QJsonArray array;
    for (const PropertyDefinition &def : definitions) {
        QJsonObject obj;
        obj[QLatin1String(JsonFields::ID)]          = def.id;
        obj[QLatin1String(JsonFields::NAME)]        = def.name;
        obj[QLatin1String(JsonFields::SUPPORTED)]   = def.isSupported;
        obj[QLatin1String(JsonFields::VALUE)]       = def.value;
        obj[QLatin1String(JsonFields::DEFAULT)]     = def.defaultValue;
        obj[QLatin1String(JsonFields::NEED_REBOOT)] = def.needReboot;
        obj[QLatin1String(JsonFields::TYPE)]        = def.type;
        obj[QLatin1String(JsonFields::READ_ONLY)]   = def.readOnly;
        array.append(obj);
    }

    QJsonObject root;
    root[QLatin1String(JsonFields::ROOT)] = array;
    const QByteArray jsonBytes = QJsonDocument(root).toJson(QJsonDocument::Indented);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMsg = file.errorString();
        return false;
    }

    if (file.write(jsonBytes) != jsonBytes.size()) {
        errorMsg = file.errorString();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// IPropertyDefinitionExporter — import from JSON
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::importFromFile(const QString &filePath,
                                               QVector<PropertyDefinition> &outDefinitions,
                                               QString &errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        errorMsg = tr("Invalid file format: root element must be a JSON object.");
        return false;
    }

    const QJsonValue rootVal = doc.object().value(QLatin1String(JsonFields::ROOT));
    if (!rootVal.isArray()) {
        errorMsg = tr("Invalid file format: '%1' array not found.")
                       .arg(QLatin1String(JsonFields::ROOT));
        return false;
    }

    QVector<PropertyDefinition> imported;
    const QJsonArray array = rootVal.toArray();
    imported.reserve(array.size());

    for (const QJsonValue &val : array) {
        if (!val.isObject())
            continue;

        const QJsonObject obj = val.toObject();
        const QString name = obj.value(QLatin1String(JsonFields::NAME)).toString().trimmed();
        if (name.isEmpty())
            continue;

        PropertyDefinition def;
        def.id           = obj.value(QLatin1String(JsonFields::ID)).toString();
        def.name         = name;
        def.isSupported  = obj.value(QLatin1String(JsonFields::SUPPORTED)).toBool();
        def.value        = obj.value(QLatin1String(JsonFields::VALUE)).toString();
        def.defaultValue = obj.value(QLatin1String(JsonFields::DEFAULT)).toString();
        def.needReboot   = obj.value(QLatin1String(JsonFields::NEED_REBOOT)).toBool();
        def.type         = obj.value(QLatin1String(JsonFields::TYPE)).toString();
        def.readOnly     = obj.value(QLatin1String(JsonFields::READ_ONLY)).toBool();
        imported.append(def);
    }

    if (imported.isEmpty()) {
        errorMsg = tr("File contained no valid property definitions.");
        return false;
    }

    outDefinitions = std::move(imported);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Status query
// ─────────────────────────────────────────────────────────────────────────────

bool PropertyDefinitionBackend::isDatabaseOpen() const
{
    return m_dbOpen;
}
