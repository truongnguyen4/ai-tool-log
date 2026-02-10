#ifndef PROPERTYDEFINITION_H
#define PROPERTYDEFINITION_H

#include <QString>

struct PropertyDefinition {
    QString id;
    QString name;
    bool isSupported;
    bool isLoaded;
    bool need_reboot;
    bool volatile_;
    bool read_only;
    bool optional;
    QString persistence;
    QString eManager;
    QString value;
    
    PropertyDefinition()
        : isSupported(false)
        , isLoaded(false)
        , need_reboot(false)
        , volatile_(false)
        , read_only(false)
        , optional(false)
    {}
    
    bool isValid() const {
        return !name.isEmpty();
    }
};

#endif // PROPERTYDEFINITION_H
