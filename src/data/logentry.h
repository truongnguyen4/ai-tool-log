#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <QString>
#include <QtGlobal>

struct LogEntry {
    quint64 id = 0;   // unique monotonically-increasing ID assigned at ingestion time
    QString date;
    QString time;
    QString pid;
    QString tid;
    QString package;
    QString level;
    QString tag;
    QString message;
    
    bool isValid() const {
        return !level.isEmpty() && !message.isEmpty();
    }
};

#endif // LOGENTRY_H
