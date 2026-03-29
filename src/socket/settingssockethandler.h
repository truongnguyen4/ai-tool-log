#ifndef SETTINGSSOCKETHANDLER_H
#define SETTINGSSOCKETHANDLER_H

#include <QObject>
#include <QVector>
#include "isocketdatahandler.h"
#include "settingentry.h"

/**
 * SettingsSocketHandler
 *
 * Handles socket messages in simple key:value format.
 *
 * Expected format — one entry per line (each line is a separate parseData() call):
 *   airplane_mode_on:0
 *   wifi_enabled:1
 *
 * The group field is intentionally omitted from the wire format.  The model
 * will search all groups for the first matching setting name.
 *
 * On a successful parse it emits settingsReceived() so that interested
 * parties (e.g. UiManager) can refresh the settings table.
 */
class SettingsSocketHandler : public QObject, public ISocketDataHandler
{
    Q_OBJECT

public:
    explicit SettingsSocketHandler(QObject *parent = nullptr);

    void parseData(const QString &type, const QByteArray &message) override;

signals:
    void settingsReceived(const QVector<SettingEntry> &settings);
};

#endif // SETTINGSSOCKETHANDLER_H
