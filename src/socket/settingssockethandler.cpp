#include "settingssockethandler.h"

#include <QDebug>

SettingsSocketHandler::SettingsSocketHandler(QObject *parent)
    : QObject(parent)
{
}

void SettingsSocketHandler::parseData(const QString &type, const QByteArray &message)
{
    if (type.compare("settings", Qt::CaseInsensitive) != 0)
        return;

    const QString line = QString::fromUtf8(message).trimmed();
    if (line.isEmpty())
        return;

    // Expected payload: (code)key:value  OR  key:value
    QString payload = line;
    QString code;
    if (payload.startsWith('(')) {
        const int close = payload.indexOf(')');
        if (close > 1) {
            code    = payload.mid(1, close - 1).trimmed();
            payload = payload.mid(close + 1).trimmed();
        }
    }

    // The colon is the separator.  The value may itself contain colons.
    const int sep = payload.indexOf(':');
    if (sep <= 0) {
        qWarning() << "SettingsSocketHandler: invalid format (missing ':'):"
                   << line.left(80);
        return;
    }

    SettingEntry entry;
    entry.group   = QString(); // unknown — model will search all groups
    entry.setting = payload.left(sep).trimmed();
    entry.value   = payload.mid(sep + 1);  // preserve everything after first ':'
    entry.line    = entry.setting + ":" + entry.value;
    entry.code    = code;

    if (entry.setting.isEmpty())
        return;

    qDebug() << "SettingsSocketHandler: parsed code=" << entry.code
             << entry.setting << "=" << entry.value;
    emit settingsReceived({entry});
}
