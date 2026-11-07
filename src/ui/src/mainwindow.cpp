#include "mainwindow.h"

/*
 * mainwindow.cpp
 * Purpose:
 *   Implements the MainWindow class, the top-level window of the
 *   Groundstation app. Everything the user sees (panels, telemetry
 *   readouts, plots, logs, controls) lives inside or is created by
 *   this window.
 *
 * What it does:
 *   - Builds the dashboard in the constructor: header and phase track
 *     across the top, status handheld / altitude scene / live data in
 *     the middle, plots and the GPS map along the bottom.
 *   - Column widths are fixed proportions of the window, so panels
 *     don't shift when the numbers in them change width.
 *   - Owns the child widgets, so Qt cleans them up automatically when
 *     the window closes.
 *   - Receives each telemetry packet in onTelemetry() and hands it to
 *     every widget. Frames come from the backend's TelemetryHub, already
 *     decoded; telemetryFromFrame() puts them in display units.
 *
 */

#include "altitudescene.h"
#include "gpsmap.h"
#include "groundstationbackend.h"
#include "headerbar.h"
#include "lineplot.h"
#include "livedatapanel.h"
#include "pagebackground.h"
#include "phasetrack.h"
#include "statusscreen.h"
#include "theme.h"

#include <QGridLayout>
#include <QWidget>

#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 600

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    resize(SCREEN_WIDTH, SCREEN_HEIGHT);
    setWindowTitle("MetRocketry Groundstation");

    // Central widget: Peach's castle behind everything
    auto *central = new PageBackground(this);
    central->setStyleSheet(Theme::styleSheet());

    auto *page = new QGridLayout(central);
    page->setContentsMargins(12, 8, 12, 12);
    page->setSpacing(10);  // gaps between panels, so the background shows through

    // Top: header and phase track span all three columns
    m_header = new HeaderBar;
    m_phaseTrack = new PhaseTrack;
    page->addWidget(m_header, 0, 0, 1, 3);
    page->addWidget(m_phaseTrack, 1, 0, 1, 3);

    // Middle: status handheld | altitude scene | live data
    m_status = new StatusScreen;
    m_scene = new AltitudeScene;
    m_liveData = new LiveDataPanel;
    page->addWidget(m_status, 2, 0);
    page->addWidget(m_scene, 2, 1);
    page->addWidget(m_liveData, 2, 2);

    // Bottom: 2x2 plots under the first two columns, GPS under live data
    auto *plots = new QGridLayout;
    plots->setSpacing(10);
    m_altitudePlot = new LinePlot(QStringLiteral("Altitude (m)"), 0, 800);
    m_altitudePlot->addSeries(QStringLiteral("ALT"), Theme::Blue);
    m_temperaturePlot = new LinePlot(QStringLiteral("Temperature (°C)"), 0, 30);
    m_temperaturePlot->addSeries(QStringLiteral("TEMP"), Theme::Red);
    m_pressurePlot = new LinePlot(QStringLiteral("Pressure (kPa)"), 90, 105);
    m_pressurePlot->addSeries(QStringLiteral("PRESS"), Theme::Blue);
    m_orientationPlot = new LinePlot(QStringLiteral("Orientation (°)"), -180, 180);
    m_rollSeries = m_orientationPlot->addSeries(QStringLiteral("R"), Theme::Red);
    m_pitchSeries = m_orientationPlot->addSeries(QStringLiteral("P"), Theme::Green, Qt::DashLine);
    m_yawSeries = m_orientationPlot->addSeries(QStringLiteral("Y"), Theme::Blue, Qt::DotLine);
    plots->addWidget(m_altitudePlot, 0, 0);
    plots->addWidget(m_temperaturePlot, 0, 1);
    plots->addWidget(m_pressurePlot, 1, 0);
    plots->addWidget(m_orientationPlot, 1, 1);
    page->addLayout(plots, 3, 0, 1, 2);

    m_gps = new GpsMap;
    page->addWidget(m_gps, 3, 2);

    // Width comes only from the stretch factors below, never from the
    // text inside a panel. Without this, a value getting wider (e.g. a yaw
    // of -161) grows its panel and the whole page reflows.
    for (QWidget *panel : std::initializer_list<QWidget *>{m_status, m_scene, m_liveData, m_gps,
                                                           m_altitudePlot, m_temperaturePlot,
                                                           m_pressurePlot, m_orientationPlot}) {
        panel->setSizePolicy(QSizePolicy::Ignored, panel->sizePolicy().verticalPolicy());
    }
    page->setColumnStretch(0, 10);
    page->setColumnStretch(1, 9);
    page->setColumnStretch(2, 7);
    plots->setColumnStretch(0, 1);
    plots->setColumnStretch(1, 1);

    // Plots get the most height
    page->setRowStretch(2, 4);
    page->setRowStretch(3, 6);

    setCentralWidget(central);
}

MainWindow::~MainWindow() = default;

void MainWindow::connectBackend(GroundstationBackend *backend, bool simulated)
{
    m_backend = backend;
    m_simulated = simulated;

    connect(backend->hub(), &TelemetryHub::frameAdded, this, &MainWindow::onFrame);
    connect(backend, &GroundstationBackend::linkChanged, this, [this](bool up, const QString &description) {
        m_header->setStatus(description);
        m_status->setLinkLabel(!up ? QStringLiteral("NO LINK")
                                   : (m_simulated ? QStringLiteral("SIM") : QStringLiteral("LIVE")));
    });
    connect(backend, &GroundstationBackend::signalQuality, this, [this](int rssiDbm, double) {
        m_status->setSignal(rssiDbm);
    });
    connect(backend, &GroundstationBackend::packetRejected, this, [this](quint64 total) {
        m_rejected = total;
    });
    connect(backend, &GroundstationBackend::logFailed, this, [this](const QString &message) {
        m_status->setLogging(false);
        m_header->setStatus(message);
    });
    connect(backend, &GroundstationBackend::udpFailed, m_header, &HeaderBar::setStatus);

    // The worker opens a log as soon as it starts
    m_status->setLogging(true);
    m_status->setLinkLabel(QStringLiteral("NO LINK"));

    Telemetry idle;
    idle.simulated = simulated;
    m_header->updateTelemetry(idle);
}

void MainWindow::onFrame(const TelemetryFrame &frame)
{
    Telemetry telemetry = telemetryFromFrame(frame);
    telemetry.packetsReceived = m_backend->hub()->frameCount();
    telemetry.packetsLost = m_rejected;
    telemetry.simulated = m_simulated;
    onTelemetry(telemetry);
}

void MainWindow::onTelemetry(const Telemetry &telemetry)
{
    const double t = telemetry.missionTime;

    // A new flight: the clock went backwards (reboot) or the rocket is
    // back on the pad after flying (the sim's loop)
    if (t < m_lastTime || (telemetry.flightState == StateIdle && m_lastState != StateIdle))
        clearFlight();
    m_lastTime = t;

    const QList<LinePlot *> plots = {m_altitudePlot, m_temperaturePlot, m_pressurePlot, m_orientationPlot};
    auto logEvent = [&](const QString &name, const QString &shortName) {
        m_scene->setLastEvent(QStringLiteral("%1 %2 ✓").arg(formatMissionTime(t), name));
        for (LinePlot *plot : plots)
            plot->addEvent(t, shortName);
    };

    if (telemetry.flightState != m_lastState) {
        logEvent(flightStateName(telemetry.flightState), stateShortName(telemetry.flightState));
        m_lastState = telemetry.flightState;
    }
    if (telemetry.landed() && !m_landed)
        logEvent(QStringLiteral("LANDED"), QStringLiteral("LANDED"));
    m_landed = telemetry.landed();

    for (LinePlot *plot : plots)
        plot->setFlying(!telemetry.onGround());

    m_header->updateTelemetry(telemetry);
    m_phaseTrack->setState(telemetry.flightState, telemetry.landed());
    m_status->updateTelemetry(telemetry);
    m_scene->addSample(telemetry);
    m_liveData->updateTelemetry(telemetry);
    m_gps->updateTelemetry(telemetry);

    m_altitudePlot->append(0, t, telemetry.altitude);
    m_temperaturePlot->append(0, t, telemetry.temperature);
    m_pressurePlot->append(0, t, telemetry.pressure);
    m_orientationPlot->append(m_rollSeries, t, telemetry.roll);
    m_orientationPlot->append(m_pitchSeries, t, telemetry.pitch);
    m_orientationPlot->append(m_yawSeries, t, telemetry.yaw);
}

void MainWindow::clearFlight()
{
    m_scene->clear();
    m_gps->clear();
    m_altitudePlot->clear();
    m_temperaturePlot->clear();
    m_pressurePlot->clear();
    m_orientationPlot->clear();
    m_lastState = StateIdle;
    m_landed = false;
}
