#ifndef SIMSOURCE_H
#define SIMSOURCE_H

#include <QTimer>

#include "telemetryframe.h"
#include "telemetrysource.h"

/*
 * simsource.h
 * Purpose:
 *   Test telemetry for the demo stage (rule 15). Generates a looping
 *   rocket flight and emits it as real wire-format lines, so the parser,
 *   hub and logger run exactly as they do in flight.
 */

class SimSource : public TelemetrySource
{
    Q_OBJECT

public:
    explicit SimSource(int intervalMs, QObject *parent = nullptr);

    void start() override;
    void stop() override;

    // The simulated rocket at a moment in the (looping) flight.
    static TelemetryFrame frameAt(quint32 elapsedMs);

private:
    void tick();

    QTimer m_timer;
    int m_intervalMs;
    quint32 m_sequence = 0;
};

#endif // SIMSOURCE_H
