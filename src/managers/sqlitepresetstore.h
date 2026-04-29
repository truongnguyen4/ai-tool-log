#ifndef SQLITEPRESETSTORE_H
#define SQLITEPRESETSTORE_H

#include "presetstore.h"
#include <QSqlDatabase>

/**
 * SqlitePresetStore
 *
 * SQLite-backed PresetStore. Schema:
 *   presets(category TEXT, name TEXT, payload BLOB, updated_at DATETIME,
 *           UNIQUE(category, name))
 *
 * The `category` column lets multiple unrelated preset families share one
 * database file. Each instance is bound to one category at construction.
 *
 * The DB file lives at:
 *   QStandardPaths::AppDataLocation / dbFileName
 */
class SqlitePresetStore : public PresetStore
{
public:
    /**
     * @param category     Logical bucket inside the DB (e.g. "propertydefs").
     * @param dbFileName   File name within AppDataLocation. Multiple stores
     *                     may share the same file safely.
     */
    SqlitePresetStore(const QString &category,
                      const QString &dbFileName = QStringLiteral("presets.db"));
    ~SqlitePresetStore() override;

    QStringList listPresets() const override;
    bool savePreset(const QString &name,
                    const QByteArray &payload,
                    QString &errorMsg) override;
    QByteArray loadPreset(const QString &name) const override;
    bool deletePreset(const QString &name, QString &errorMsg) override;

    bool isOpen() const { return m_open; }

private:
    bool initDatabase(const QString &dbFileName, QString &errorMsg);
    bool createSchema(QString &errorMsg);

    QString      m_category;
    QString      m_connectionName;
    QSqlDatabase m_db;
    bool         m_open = false;
};

#endif // SQLITEPRESETSTORE_H
