#ifndef PRESETSTORE_H
#define PRESETSTORE_H

#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * PresetStore
 *
 * Generic abstraction for "named blob" persistence: save/load/list/delete a
 * set of named QByteArray payloads. Callers serialize their own typed data
 * to a QByteArray (typically JSON) and let this layer worry about storage.
 *
 * Two backends ship today:
 *   - SqlitePresetStore   — for larger / transactional payloads
 *   - QSettingsPresetStore — for small payloads piggy-backing on the existing
 *                             QSettings file
 */
class PresetStore
{
public:
    virtual ~PresetStore() = default;

    /** Names of all stored presets, sorted alphabetically. */
    virtual QStringList listPresets() const = 0;

    /** Insert or overwrite a preset.  Returns false + sets errorMsg on failure. */
    virtual bool savePreset(const QString &name,
                            const QByteArray &payload,
                            QString &errorMsg) = 0;

    /** Load a preset's payload. Returns empty QByteArray if not found. */
    virtual QByteArray loadPreset(const QString &name) const = 0;

    /** Delete the named preset. Returns false + sets errorMsg on failure. */
    virtual bool deletePreset(const QString &name, QString &errorMsg) = 0;
};

#endif // PRESETSTORE_H
