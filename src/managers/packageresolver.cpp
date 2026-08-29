#include "packageresolver.h"

#include "adbcommand.h"
#include "adbexecutor.h"
#include "adbmanager.h"

#include <QMetaObject>
#include <QRegularExpression>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

namespace {
/** How often the process table is re-read while a capture runs. */
constexpr int kPollIntervalMs = 5000;
constexpr int kTimeoutMs      = 5000;
/** Classic `ps` output: USER PID PPID VSIZE RSS WCHAN PC NAME. */
constexpr int kClassicColumns = 8;
} // namespace

PackageResolver::PackageResolver(QObject *parent)
    : QObject(parent)
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(kPollIntervalMs);
    connect(m_pollTimer, &QTimer::timeout, this, &PackageResolver::refresh);
}

void PackageResolver::setDevice(const QString &serial)
{
    if (m_serial == serial)
        return;
    m_serial = serial;
    if (!m_processByPid.isEmpty()) {
        m_processByPid.clear();
        emit mappingChanged();
    }
    refresh();
}

void PackageResolver::setPolling(bool on)
{
    if (on) {
        refresh();
        m_pollTimer->start();
    } else {
        m_pollTimer->stop();
    }
}

QStringList PackageResolver::packages() const
{
    QStringList names = QStringList(QSet<QString>(m_processByPid.cbegin(), m_processByPid.cend())
                                        .values());
    names.sort(Qt::CaseInsensitive);
    return names;
}

void PackageResolver::refresh()
{
    if (m_serial.isEmpty() || m_inFlight)
        return;
    m_inFlight = true;

    const QString serial  = m_serial;
    const QString adbPath = AdbManager::instance().getAdbPath();
    (void)QtConcurrent::run([this, serial, adbPath]() {
        const AdbProcessResult result =
            AdbExecutor::run(adbPath, AdbCommand::listProcesses(serial), kTimeoutMs);
        QHash<QString, QString> processes = parseProcessList(result.standardOutput);

        QMetaObject::invokeMethod(this, [this, serial, processes]() {
            m_inFlight = false;
            // The device changed under the read, or it answered nothing usable.
            if (serial != m_serial || processes.isEmpty())
                return;
            if (processes == m_processByPid)
                return;
            m_processByPid = processes;
            emit mappingChanged();
        }, Qt::QueuedConnection);
    });
}

QHash<QString, QString> PackageResolver::parseProcessList(const QString &output)
{
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    static const QRegularExpression digits(QStringLiteral("^\\d+$"));

    QHash<QString, QString> processes;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList columns = line.trimmed().split(whitespace, Qt::SkipEmptyParts);
        // `ps -A -o PID,NAME`, the header line included.
        if (columns.size() == 2 && digits.match(columns.at(0)).hasMatch()) {
            processes.insert(columns.at(0), columns.at(1));
            continue;
        }
        // Older devices print the classic columns; the pid is the second.
        if (columns.size() >= kClassicColumns && digits.match(columns.at(1)).hasMatch())
            processes.insert(columns.at(1), columns.constLast());
    }
    return processes;
}
