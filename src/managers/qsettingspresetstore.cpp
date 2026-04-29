#include "qsettingspresetstore.h"

#include <QList>
#include <QPair>

QSettingsPresetStore::QSettingsPresetStore(const QString &groupName, QSettings *settings)
    : m_groupName(groupName)
    , m_external(settings)
{
}

QSettings &QSettingsPresetStore::settings() const
{
    return m_external ? *m_external : m_owned;
}

QStringList QSettingsPresetStore::listPresets() const
{
    QSettings &s = settings();
    QStringList names;
    const int count = s.beginReadArray(m_groupName);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        names << s.value(QStringLiteral("name")).toString();
    }
    s.endArray();
    std::sort(names.begin(), names.end());
    return names;
}

bool QSettingsPresetStore::savePreset(const QString &name,
                                      const QByteArray &payload,
                                      QString &errorMsg)
{
    Q_UNUSED(errorMsg);
    QSettings &s = settings();

    // Read existing presets, dropping any with the same name (we are overwriting).
    QList<QPair<QString, QByteArray>> presets;
    const int count = s.beginReadArray(m_groupName);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        const QString n = s.value(QStringLiteral("name")).toString();
        const QByteArray d = s.value(QStringLiteral("data")).toByteArray();
        if (n != name)
            presets.append({n, d});
    }
    s.endArray();

    presets.append({name, payload});

    s.beginWriteArray(m_groupName, presets.size());
    for (int i = 0; i < presets.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"), presets[i].first);
        s.setValue(QStringLiteral("data"), presets[i].second);
    }
    s.endArray();
    s.sync();
    return true;
}

QByteArray QSettingsPresetStore::loadPreset(const QString &name) const
{
    QSettings &s = settings();
    QByteArray result;
    const int count = s.beginReadArray(m_groupName);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        if (s.value(QStringLiteral("name")).toString() == name) {
            result = s.value(QStringLiteral("data")).toByteArray();
            break;
        }
    }
    s.endArray();
    return result;
}

bool QSettingsPresetStore::deletePreset(const QString &name, QString &errorMsg)
{
    Q_UNUSED(errorMsg);
    QSettings &s = settings();

    QList<QPair<QString, QByteArray>> presets;
    const int count = s.beginReadArray(m_groupName);
    for (int i = 0; i < count; ++i) {
        s.setArrayIndex(i);
        const QString n = s.value(QStringLiteral("name")).toString();
        if (n != name) {
            presets.append({n, s.value(QStringLiteral("data")).toByteArray()});
        }
    }
    s.endArray();

    s.remove(m_groupName);
    s.beginWriteArray(m_groupName, presets.size());
    for (int i = 0; i < presets.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("name"), presets[i].first);
        s.setValue(QStringLiteral("data"), presets[i].second);
    }
    s.endArray();
    s.sync();
    return true;
}
