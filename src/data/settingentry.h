#ifndef SETTINGENTRY_H
#define SETTINGENTRY_H

#include <QString>

struct SettingEntry {
    QString line;
    QString group;
    QString setting;
    QString value;
    QString code;    // optional setting code from socket message, e.g. "1000" from "(1000)key:value"
};

#endif // SETTINGENTRY_H
