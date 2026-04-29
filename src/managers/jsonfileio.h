#ifndef JSONFILEIO_H
#define JSONFILEIO_H

#include <QJsonDocument>
#include <QString>

/**
 * JsonFileIo
 *
 * Tiny helpers for reading/writing a QJsonDocument to disk. Centralizes the
 * file-open / write-all / parse boilerplate that was duplicated across the
 * codebase.
 */
namespace JsonFileIo {

/** Write @p doc (indented) to @p filePath. Returns false + sets @p errorMsg on failure. */
bool writeFile(const QString &filePath,
               const QJsonDocument &doc,
               QString &errorMsg);

/** Read @p filePath and parse into @p outDoc. Returns false + sets @p errorMsg on failure. */
bool readFile(const QString &filePath,
              QJsonDocument &outDoc,
              QString &errorMsg);

} // namespace JsonFileIo

#endif // JSONFILEIO_H
