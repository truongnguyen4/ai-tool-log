#ifndef PROPERTYDEFINITION_H
#define PROPERTYDEFINITION_H

#include <QString>

struct PropertyDefinition {
    QString id;
    QString name;
    bool isSupported;
    bool isLoaded;
    bool needReboot;
    bool isVolatile;
    bool readOnly;
    bool optional;
    QString persistence;
    QString value;
    
    // Parsing keys for ADB command output
    static inline const QString KEY_ID = "id";
    static inline const QString KEY_NAME = "name";
    static inline const QString KEY_OPTIONAL = "optional";
    static inline const QString KEY_PERSISTENCE = "persistence";
    static inline const QString KEY_VOLATILE = "volatile";
    static inline const QString KEY_IS_SUPPORTED = "issupported";
    static inline const QString KEY_IS_SUPPORTED_ALT = "is_supported";
    static inline const QString KEY_VALUE = "value";
    static inline const QString KEY_NEED_REBOOT = "needreboot";
    static inline const QString KEY_NEED_REBOOT_ALT = "need_reboot";
    static inline const QString KEY_READ_ONLY = "readonly";
    static inline const QString KEY_READ_ONLY_ALT = "read_only";
    static inline const QString KEY_IS_LOADED = "isloaded";
    static inline const QString KEY_IS_LOADED_ALT = "is_loaded";
    
    PropertyDefinition()
        : isSupported(false)
        , isLoaded(false)
        , needReboot(false)
        , isVolatile(false)
        , readOnly(false)
        , optional(false)
    {}
    
    bool isValid() const {
        return !name.isEmpty();
    }
};

#endif // PROPERTYDEFINITION_H
