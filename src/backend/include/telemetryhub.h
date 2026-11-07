#ifndef TELEMETRYHUB_H
#define TELEMETRYHUB_H

#include <QList>
#include <QObject>

#include "telemetryframe.h"

/*
 * telemetryhub.h
 * Purpose:
 *   The UI's view of telemetry. Lives on the UI thread and keeps recent
 *   history for the live plots. Connect to frameAdded for updates; the
 *   complete record is on disk in TelemetryLogger.
 */

class TelemetryHub : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    const QList<TelemetryFrame> &history() const { return m_history; }
    quint64 frameCount() const { return m_frameCount; }

public slots:
    void addFrame(const TelemetryFrame &frame);

signals:
    void frameAdded(const TelemetryFrame &frame);

private:
    QList<TelemetryFrame> m_history;
    quint64 m_frameCount = 0;
};

#endif // TELEMETRYHUB_H
