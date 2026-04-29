#include "jsonfileio.h"

#include <QFile>
#include <QJsonParseError>

namespace JsonFileIo {

bool writeFile(const QString &filePath, const QJsonDocument &doc, QString &errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMsg = file.errorString();
        return false;
    }
    const QByteArray bytes = doc.toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        errorMsg = file.errorString();
        return false;
    }
    return true;
}

bool readFile(const QString &filePath, QJsonDocument &outDoc, QString &errorMsg)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        errorMsg = file.errorString();
        return false;
    }
    QJsonParseError parseError;
    outDoc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        errorMsg = parseError.errorString();
        return false;
    }
    return true;
}

} // namespace JsonFileIo
