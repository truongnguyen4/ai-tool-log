#ifndef QSETTINGSPRESETSTORE_H
#define QSETTINGSPRESETSTORE_H

#include "presetstore.h"
#include <QSettings>

/**
 * QSettingsPresetStore
 *
 * QSettings-backed PresetStore. Stores presets as a beginWriteArray under a
 * given group name. Suited to small payloads (~kilobytes) that piggy-back on
 * the existing application config file.
 *
 * Layout:
 *   <groupName>/size = N
 *   <groupName>/1/name, <groupName>/1/data
 *   ...
 */
class QSettingsPresetStore : public PresetStore
{
public:
    /**
     * @param groupName  QSettings array group (e.g. "ConfigPresets").
     * @param settings   Optional shared QSettings; if null, the store opens
     *                   its own (using QCoreApplication org/app names).
     */
    explicit QSettingsPresetStore(const QString &groupName,
                                  QSettings *settings = nullptr);

    QStringList listPresets() const override;
    bool savePreset(const QString &name,
                    const QByteArray &payload,
                    QString &errorMsg) override;
    QByteArray loadPreset(const QString &name) const override;
    bool deletePreset(const QString &name, QString &errorMsg) override;

private:
    QSettings &settings() const;

    QString               m_groupName;
    QSettings            *m_external = nullptr;     // not owned
    mutable QSettings     m_owned;                  // used iff m_external is null
};

#endif // QSETTINGSPRESETSTORE_H
