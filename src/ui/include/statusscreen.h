#ifndef STATUSSCREEN_H
#define STATUSSCREEN_H

/*
 * statusscreen.h
 * Purpose:
 *   The rocket's status, shown on the LCD of a Game & Watch style
 *   handheld: photo, flight state, packet count, radio signal strength,
 *   a few readouts, and one LCD segment per state. The handheld's
 *   light blinks on every packet.
 */

#include "telemetry.h"

#include <QWidget>

class HandheldShell;
class PhaseSegments;
class QLabel;
class QProgressBar;

class StatusScreen : public QWidget
{
    Q_OBJECT

public:
    explicit StatusScreen(QWidget *parent = nullptr);

    void updateTelemetry(const Telemetry &telemetry);
    void setLogging(bool logging);
    void setSignal(int rssiDbm);
    // linkLabel: SIM, LIVE or NO LINK
    void setLinkLabel(const QString &linkLabel);

private:
    HandheldShell *m_shell;
    QLabel *m_source;
    QLabel *m_phase;
    QLabel *m_packets;
    QProgressBar *m_signal;
    QLabel *m_rssi;
    QLabel *m_pressure;
    QLabel *m_gps;
    QLabel *m_logging;
    PhaseSegments *m_segments;
};

#endif // STATUSSCREEN_H
