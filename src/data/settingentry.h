#ifndef SETTINGENTRY_H
#define SETTINGENTRY_H

#include <QString>

struct SettingEntry {
    QString line;
    QString group;
    QString setting;
    QString value;
};

#endif // SETTINGENTRY_H
