#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "telemetry.h"

#include <QMainWindow>

class AltitudeScene;
class GroundstationBackend;
class GpsMap;
class HeaderBar;
class LinePlot;
class LiveDataPanel;
class PhaseTrack;
class StatusScreen;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Wires the dashboard to the backend. Call before backend->start().
    void connectBackend(GroundstationBackend *backend, bool simulated);

private:
    void onFrame(const TelemetryFrame &frame);
    void onTelemetry(const Telemetry &telemetry);
    void clearFlight();

    HeaderBar *m_header;
    PhaseTrack *m_phaseTrack;
    StatusScreen *m_status;
    AltitudeScene *m_scene;
    LiveDataPanel *m_liveData;
    GpsMap *m_gps;

    LinePlot *m_altitudePlot;
    LinePlot *m_temperaturePlot;
    LinePlot *m_pressurePlot;
    LinePlot *m_orientationPlot;
    int m_rollSeries;
    int m_pitchSeries;
    int m_yawSeries;

    GroundstationBackend *m_backend = nullptr;
    bool m_simulated = false;
    quint64 m_rejected = 0;

    qint32 m_lastState = StateIdle;
    bool m_landed = false;
    double m_lastTime = 0;
};
#endif // MAINWINDOW_H
