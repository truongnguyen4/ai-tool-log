#ifndef PROPERTYDEFINITIONBACKEND_H
#define PROPERTYDEFINITIONBACKEND_H

#include <QObject>
#include <QSqlDatabase>
#include "ipropertydefinitiondatabase.h"
#include "ipropertydefinitionexporter.h"

/**
 * PropertyDefinitionBackend
 *
 * Concrete implementation that satisfies both persistence interfaces:
 *   - IPropertyDefinitionDatabase  →  SQLite (via Qt SQL module)
 *   - IPropertyDefinitionExporter  →  JSON   (via QJsonDocument)
 *
 * Replacing the storage engine only requires a new subclass of either
 * interface; this class is the default "batteries-included" backend.
 *
 * The SQLite database is opened once at construction and reused for the
 * lifetime of the object.  The file is stored in
 *   QStandardPaths::AppDataLocation / "propertydefs.db"
 */
class PropertyDefinitionBackend
    : public QObject
    , public IPropertyDefinitionDatabase
    , public IPropertyDefinitionExporter
{
    Q_OBJECT

public:
    explicit PropertyDefinitionBackend(QObject *parent = nullptr);
    ~PropertyDefinitionBackend() override;

    // ── IPropertyDefinitionDatabase ──────────────────────────────────────────
    bool savePropertySet(const QString &setName,
                         const QVector<PropertyDefinition> &definitions,
                         QString &errorMsg) override;

    QVector<PropertyDefinition> loadPropertySet(const QString &setName,
                                                QString &errorMsg) override;

    QStringList listPropertySetNames() const override;

    bool deletePropertySet(const QString &setName, QString &errorMsg) override;

    // ── IPropertyDefinitionExporter ──────────────────────────────────────────
    bool exportToFile(const QString &filePath,
                      const QVector<PropertyDefinition> &definitions,
                      QString &errorMsg) override;

    bool importFromFile(const QString &filePath,
                        QVector<PropertyDefinition> &outDefinitions,
                        QString &errorMsg) override;

    /** True if the underlying database connection was opened successfully. */
    bool isDatabaseOpen() const;

private:
    bool initDatabase(QString &errorMsg);
    bool createSchema(QString &errorMsg);

    QSqlDatabase m_db;
    QString      m_connectionName;
    bool         m_dbOpen = false;
};

#endif // PROPERTYDEFINITIONBACKEND_H
