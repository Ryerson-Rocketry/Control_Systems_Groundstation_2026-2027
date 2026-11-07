#include "simsource.h"

#include <cmath>

#include <QtMath>

#include "packetparser.h"

namespace {
// Flight timeline, in seconds from power-up. Times are made up; the order
// follows the firmware's FlightState_t.
constexpr double kPadS = 5.0;
constexpr double kBurnS = 3.0;
constexpr double kApogeeHoldS = 2.0;
constexpr double kApogeeM = 700.0;
constexpr double kAscentS = 20.0;
constexpr double kDescentMps = 8.0;
constexpr double kFlightLoopS = 140.0;
constexpr double kGpsFixS = 3.0;
// URRG launch site, Torrey Farms, Potter NY (near Penn Yan); approximate
constexpr double kPadLatitudeDeg = 42.6928;
constexpr double kPadLongitudeDeg = -77.1906;
constexpr double kPadAltitudeAslM = 300.0;

qint32 flightStateAt(double seconds)
{
    if (seconds < kPadS)
        return StateIdle;
    if (seconds < kPadS + kBurnS)
        return StateBoosterBurn;
    if (seconds < kPadS + kAscentS)
        return StateCanardActivation;
    if (seconds < kPadS + kAscentS + kApogeeHoldS)
        return StateApogee;
    return StateDescent;
}

bool motorBurning(double seconds)
{
    return seconds >= kPadS && seconds < kPadS + kBurnS;
}

double altitudeAt(double seconds)
{
    const double flight = seconds - kPadS;
    if (flight <= 0.0)
        return 0.0;
    if (flight < kAscentS) {
        const double progress = flight / kAscentS;
        return kApogeeM * (1.0 - (1.0 - progress) * (1.0 - progress));
    }
    return qMax(0.0, kApogeeM - kDescentMps * (flight - kAscentS));
}

}

TelemetryFrame SimSource::frameAt(quint32 elapsedMs)
{
    const double seconds = std::fmod(elapsedMs / 1000.0, kFlightLoopS);
    const double flying = seconds > kPadS ? 1.0 : 0.0;

    RocketTelemetry data;
    data.timestampMs = elapsedMs;
    data.altitudeM = altitudeAt(seconds);
    data.velocityMps = (altitudeAt(seconds + 0.05) - altitudeAt(seconds - 0.05)) / 0.1;
    data.flightState = flightStateAt(seconds);
    data.pressurePa = 101325.0 * qPow(1.0 - 2.25577e-5 * data.altitudeM, 5.25588);
    data.temperatureC = 20.0 - 0.0065 * data.altitudeM;

    data.accelMg = {15.0 * qSin(seconds), 12.0 * qCos(seconds), motorBurning(seconds) ? 6000.0 : 1000.0};
    data.gyroDps = {flying * 30.0 * qSin(seconds * 2.0), flying * 30.0 * qCos(seconds * 2.0),
                    flying * 90.0};
    data.magnetometer = {20.0 * qCos(seconds), 20.0 * qSin(seconds), -45.0};

    data.rollDeg = std::remainder(flying * 90.0 * (seconds - kPadS), 360.0);
    data.pitchDeg = flying * 3.0 * qSin(seconds * 0.7);
    data.yawDeg = flying * 3.0 * qCos(seconds * 0.7);
    data.tiltAngleDeg = std::hypot(data.pitchDeg, data.yawDeg);

    // The control loop only runs while the canards are active.
    if (data.flightState == StateCanardActivation) {
        data.rollErrorDeg = 2.0 * qSin(seconds * 1.3);
        data.pitchErrorDeg = 1.5 * qCos(seconds * 1.1);
        data.pwmAngleDeg = -4.0 * data.rollErrorDeg;
    }

    // The receiver needs a few seconds after power-up to get a fix.
    if (seconds >= kGpsFixS) {
        const double drift = flying * (seconds - kPadS);
        data.gps.hasFix = true;
        data.gps.latitudeDeg = kPadLatitudeDeg + 0.00002 * drift;
        data.gps.longitudeDeg = kPadLongitudeDeg + 0.00003 * drift;
        data.gps.altitudeM = kPadAltitudeAslM + data.altitudeM;
        data.gps.speedKmh = std::abs(data.velocityMps) * 3.6;
    }

    TelemetryFrame frame;
    frame.data = data;
    return frame;
}

SimSource::SimSource(int intervalMs, QObject *parent)
    : TelemetrySource(parent), m_timer(this), m_intervalMs(intervalMs)
{
    m_timer.setInterval(intervalMs);
    connect(&m_timer, &QTimer::timeout, this, &SimSource::tick);
}

void SimSource::start()
{
    m_timer.start();
    emit linkChanged(true, QStringLiteral("SIMULATED telemetry"));
}

void SimSource::stop()
{
    m_timer.stop();
    emit linkChanged(false, QStringLiteral("Simulator stopped"));
}

void SimSource::tick()
{
    const quint32 elapsedMs = m_sequence * static_cast<quint32>(m_intervalMs);
    emit bytesReceived(PacketParser::encode(frameAt(elapsedMs)));
    emit signalQuality(-70 - int(m_sequence % 8), 8.5);
    ++m_sequence;
}
