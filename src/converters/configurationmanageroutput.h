#ifndef CONFIGURATIONMANAGEROUTPUT_H
#define CONFIGURATIONMANAGEROUTPUT_H

#include <QByteArray>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

#include "propertydefinition.h"

/** What a `set` or `reset` run reported, property by property. */
struct PropertyWriteResult
{
    QStringList done;                           ///< names the service confirmed
    QVector<QPair<QString, QString>> failed;    ///< name, reason
    QString error;                              ///< the command as a whole failed

    bool succeeded() const { return error.isEmpty() && failed.isEmpty(); }
    void append(const PropertyWriteResult &other);
};

/**
 * Reads the output of the device's `cmd configuration_manager` shell command
 * and builds its arguments.
 *
 *   list --json                  every property with its metadata and value
 *   set <name> <value> ...       any number of pairs
 *   reset <name> ...             read-only and unavailable ones are skipped
 *
 * `get` is not used: one property whose getter throws aborts the whole
 * command, while `list --json` reports every value.
 */
namespace ConfigurationManagerOutput {

/** Definitions from `list --json`; empty, with @p error set, on failure. */
QVector<PropertyDefinition> parseList(const QByteArray &output, QString *error);

/** "[0, 1, ..., 27]" as 0..27, "[0, 2, 8, 10]" as those four. */
QVector<qint64> parseValuesRestriction(const QString &text);

/** "f" escapes, as the service prints char defaults, as the characters. */
QString decodeUnicodeEscapes(const QString &text);

/** Output of `set` or `reset`. */
PropertyWriteResult parseWriteOutput(const QString &output);

/**
 * @p value quoted for the device shell, which adb hands the joined command
 * line to. Plain words stay as they are.
 */
QString shellQuote(const QString &value);

/** `set` arguments, split into runs that keep each command line short. */
QVector<QStringList> setCommands(const QVector<QPair<QString, QString>> &values);
/** `reset` arguments, split likewise. */
QVector<QStringList> resetCommands(const QStringList &names);

} // namespace ConfigurationManagerOutput

#endif // CONFIGURATIONMANAGEROUTPUT_H
