#ifndef DEVICEDETAILSCONVERTER_H
#define DEVICEDETAILSCONVERTER_H

#include <QString>

/**
 * Parsers for the various adb-shell command outputs used to populate the
 * Devices tab dashboard (battery / IP / kernel / wifi status).  Each helper
 * is a pure function: trim & regex-match the supplied raw output.
 */
class DeviceDetailsConverter
{
public:
    // "level: 87" → "87%". Returns empty QString if no level found.
    static QString convertBatteryLevel(const QString &dumpsysBatteryOutput);

    // "src 192.168.1.42 ..." → "192.168.1.42". Returns empty QString if no
    // src field is found.
    static QString convertIpFromRoute(const QString &ipRouteOutput);

    // "Linux version 5.10.157-..." → "5.10.157-...". Falls back to the first
    // 60 chars of the input if the "Linux version" prefix is missing.
    static QString convertKernelVersion(const QString &procVersionOutput);

    // wifi.interface state from `dumpsys wifi` reduced to "Connected" /
    // "Disconnected" depending on whether the trimmed output equals "1".
    static QString convertWifiStatus(const QString &wifiStatusOutput);
};

#endif // DEVICEDETAILSCONVERTER_H
