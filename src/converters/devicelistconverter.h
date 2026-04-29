#ifndef DEVICELISTCONVERTER_H
#define DEVICELISTCONVERTER_H

#include <QList>
#include <QString>
#include "adbmanager.h"  // for AdbDevice

/**
 * Converter for `adb devices -l` output.
 *
 * Sample line format:
 *   <serial>\t<state> [usb:... product:... model:... device:...]
 * The first non-header line per device contains a serial and a state
 * (e.g. "device", "offline", "unauthorized").  Optional `model:` token
 * provides a friendly name.
 */
class DeviceListConverter
{
public:
    // Parses the full stdout of `adb devices -l` into a list of online
    // devices.  Devices whose state is not "device" are filtered out (this
    // matches the previous in-line behaviour of AdbManager::parseDeviceList).
    static QList<AdbDevice> convert(const QString &output);
};

#endif // DEVICELISTCONVERTER_H
