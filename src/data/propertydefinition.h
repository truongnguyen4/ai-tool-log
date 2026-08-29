#ifndef PROPERTYDEFINITION_H
#define PROPERTYDEFINITION_H

#include <QCoreApplication>
#include <QString>
#include <QVector>

/**
 * One property of the device's configuration_manager service, as
 * `cmd configuration_manager list --json` describes it.
 *
 * Values are held as the text the service prints and accepts: booleans as
 * true / false, numbers in decimal, a char as the character itself, strings
 * and blobs verbatim.
 */
struct PropertyDefinition
{
    Q_DECLARE_TR_FUNCTIONS(PropertyDefinition)

public:
    enum class Kind : quint8 {
        Boolean,
        Integer,
        Enum,         ///< one of allowedValues
        MultiChoice,  ///< one of allowedValues
        Char,
        String,
        Blob,         ///< structured value serialized as text
        Other,        ///< a type this tool does not know yet; edited as text
    };

    int     id = -1;
    QString name;
    QString type;             ///< as reported: boolean, integer, enum, ...
    QString typeRestriction;  ///< Java class behind an enum or blob; may be empty
    QVector<qint64> allowedValues;  ///< enum / multichoice choices; empty when unrestricted
    bool    hasRange = false;
    qint64  minimum  = 0;
    qint64  maximum  = 0;
    QString defaultValue;
    QString value;            ///< current value; empty while unavailable
    bool    readOnly   = false;
    bool    needReboot = false;
    bool    available  = true;

    bool isValid() const { return !name.isEmpty(); }
    Kind kind() const;

    /** Whether the service will accept a new value. */
    bool isWritable() const { return available && !readOnly; }
    /** Whether the current value differs from the default. */
    bool isModified() const { return available && !sameValue(value, defaultValue); }

    /**
     * Whether @p a and @p b mean the same value of this property: booleans and
     * numbers compare by meaning ("1" equals "true"), anything else exactly.
     */
    bool sameValue(const QString &a, const QString &b) const;

    /**
     * Check @p text as a new value. Returns an empty string when it is
     * acceptable and puts the canonical form in @p normalized; otherwise
     * returns why it is not.
     */
    QString validate(const QString &text, QString *normalized) const;

    /** The class name without its package: "CharacterSetMode". */
    QString restrictionName() const;
    /** A short type description for a table cell: "integer 2 – 300". */
    QString typeSummary() const;
};

#endif // PROPERTYDEFINITION_H
