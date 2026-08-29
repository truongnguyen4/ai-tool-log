// Shared blink-sweep timer setup for table models that fade row backgrounds
// after a short window. Extracted from PropertiesModel / SettingsModel /
// PropertyDefinitionModel which all had identical timer logic.
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace BlinkSweep {

/** How long a changed row stays tinted, in milliseconds. */
constexpr qint64 kBlinkDurationMs = 1000;

// Configures `timer` to periodically prune expired entries from `blinkUntil`
// and invoke `refresh()` (typically the model emits dataChanged for the
// Qt::BackgroundRole). The timer should be parented to the model, so the
// connection auto-disconnects on destruction.
inline void install(QTimer *timer,
                    QHash<QString, qint64> *blinkUntil,
                    const QElapsedTimer *clock,
                    std::function<void()> refresh)
{
    timer->setInterval(120);
    timer->setSingleShot(false);
    QObject::connect(timer, &QTimer::timeout, timer,
                     [timer, blinkUntil, clock, refresh = std::move(refresh)]() {
        if (blinkUntil->isEmpty()) { timer->stop(); return; }
        const qint64 now = clock->elapsed();
        QStringList expired;
        for (auto it = blinkUntil->constBegin(); it != blinkUntil->constEnd(); ++it)
            if (it.value() <= now) expired << it.key();
        if (expired.isEmpty()) return;
        for (const QString &k : expired) blinkUntil->remove(k);
        refresh();
        if (blinkUntil->isEmpty()) timer->stop();
    });
}

// Convenience overload for QAbstractTableModel-derived models that just want
// to repaint every row's BackgroundRole when sweep fires. Eliminates the
// identical "emit dataChanged(0..rows-1, 0..cols-1, {BackgroundRole})" lambda
// duplicated across SettingsModel / PropertiesModel / PropertyDefinitionModel.
template <typename Model>
inline void installForModel(QTimer *timer,
                            QHash<QString, qint64> *blinkUntil,
                            const QElapsedTimer *clock,
                            Model *model)
{
    install(timer, blinkUntil, clock, [model]() {
        const int rows = model->rowCount();
        if (rows <= 0) return;
        Q_EMIT model->dataChanged(model->index(0, 0),
                                  model->index(rows - 1, model->columnCount() - 1),
                                  {Qt::BackgroundRole});
    });
}

} // namespace BlinkSweep
