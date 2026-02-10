#ifndef PROPERTYDEFINITION_H
#define PROPERTYDEFINITION_H

#include <QString>

struct PropertyDefinition {
    QString id;
    QString name;
    bool isSupported;
    QString value;
    QString defaultValue;
    bool needReboot;
    QString type;
    bool readOnly;
    
    PropertyDefinition()
        : isSupported(false)
        , needReboot(false)
        , readOnly(false)
    {}
    
    bool isValid() const {
        return !name.isEmpty();
    }
};

#endif // PROPERTYDEFINITION_H
