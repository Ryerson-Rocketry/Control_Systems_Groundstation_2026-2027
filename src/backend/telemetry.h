#ifndef TELEMETRY_H
#define TELEMETRY_H

/*
 * telemetry.h
 * Purpose:
 *   What the UI reads: one packet in SI units scaled to suit the display.
 *   The backend decodes the radio into a TelemetryFrame in the rocket's own
 *   units; telemetryFromFrame() below is the one place those are converted,
 *   so no widget ever deals with mg, Pa or km/h.
 *
 * A value the packet did not carry is NaN here too. Show it with
 * formatReading(), and test it with hasValue() before doing maths on it.
 */

#include <QString>
#include <QtGlobal>

#include <cmath>

#include "telemetryframe.h"

constexpr int kStateCount = 6;

// For tight spots like the signs on the plots
inline QString stateShortName(qint32 state)
{
    switch (state) {
    case StateBoosterBurn:       return QStringLiteral("BOOST");
    case StateSeparation:        return QStringLiteral("SEP");
    case StateSustainerIgnition: return QStringLiteral("SUSTAIN");
    case StateCanardActivation:  return QStringLiteral("CANARD");
    default:                     return flightStateName(state);
    }
}

// A number for a readout, or "--" when the packet did not carry it
inline QString formatReading(double value, int decimals)
{
    if (!hasValue(value))
        return QStringLiteral("--");

    // Rounded first so a small negative shows as 0, not "-0"
    const double step = std::pow(10.0, decimals);
    const double rounded = std::round(value * step) / step;
    return QString::number(rounded == 0.0 ? 0.0 : rounded, 'f', decimals);
}

// "mm:ss", or "hh:mm:ss" when withHours is true
inline QString formatMissionTime(double seconds, bool withHours = false)
{
    const int total = static_cast<int>(seconds);
    const QString mmss = QStringLiteral("%1:%2")
                             .arg((total / 60) % 60, 2, 10, QLatin1Char('0'))
                             .arg(total % 60, 2, 10, QLatin1Char('0'));
    if (!withHours)
        return mmss;
    return QStringLiteral("%1:%2").arg(total / 3600, 2, 10, QLatin1Char('0')).arg(mmss);
}

struct Telemetry
{
    // From the rocket
    double missionTime = 0;      // s
    qint32 flightState = StateIdle;  // a FlightState; unknown values are possible

    double accelX = 0;           // m/s²
    double accelY = 0;
    double accelZ = 0;
    double gyroX = 0;            // deg/s
    double gyroY = 0;
    double gyroZ = 0;
    double magX = 0;             // mag units
    double magY = 0;
    double magZ = 0;

    double roll = 0;             // deg
    double pitch = 0;
    double yaw = 0;
    double tiltAngle = 0;        // deg

    double altitude = 0;         // m, 0 on the pad
    double velocity = 0;         // m/s, negative = falling
    double pressure = 0;         // kPa
    double temperature = 0;      // °C

    double rollError = 0;        // deg
    double pitchError = 0;       // deg
    double pwmAngle = 0;         // deg

    bool gpsHasFix = false;
    double gpsLatitude = 0;      // decimal deg, negative = south
    double gpsLongitude = 0;     // decimal deg, negative = west
    double gpsAltitude = 0;      // m ASL
    double gpsSpeed = 0;         // m/s

    // Added by the ground station
    quint64 packetsReceived = 0;
    quint64 packetsLost = 0;     // packets rejected as corrupt or unreadable
    bool simulated = false;

    // The radio packet may not carry altitude and velocity. Without them
    // the flight state is the only evidence: on the ground while Idle, and
    // never known to have landed.
    bool hasAltitude() const { return hasValue(altitude) && hasValue(velocity); }
    bool onGround() const
    {
        return hasAltitude() ? altitude < 2 && std::abs(velocity) < 1 : flightState == StateIdle;
    }
    // There's no landed state: the flight ends in Descent, so landing is
    // Descent with the rocket sitting still at ground level
    bool landed() const { return flightState == StateDescent && hasAltitude() && onGround(); }
};

inline Telemetry telemetryFromFrame(const TelemetryFrame &frame)
{
    constexpr double kMilliGToMps2 = 9.80665 / 1000.0;
    constexpr double kKmhToMps = 1.0 / 3.6;

    const RocketTelemetry &r = frame.data;
    Telemetry t;
    t.missionTime = r.timestampMs / 1000.0;
    t.flightState = r.flightState;
    t.accelX = r.accelMg.x * kMilliGToMps2;
    t.accelY = r.accelMg.y * kMilliGToMps2;
    t.accelZ = r.accelMg.z * kMilliGToMps2;
    t.gyroX = r.gyroDps.x;
    t.gyroY = r.gyroDps.y;
    t.gyroZ = r.gyroDps.z;
    t.magX = r.magnetometer.x;
    t.magY = r.magnetometer.y;
    t.magZ = r.magnetometer.z;
    t.roll = r.rollDeg;
    t.pitch = r.pitchDeg;
    t.yaw = r.yawDeg;
    t.tiltAngle = r.tiltAngleDeg;
    t.altitude = r.altitudeM;
    t.velocity = r.velocityMps;
    t.pressure = r.pressurePa / 1000.0;
    t.temperature = r.temperatureC;
    t.rollError = r.rollErrorDeg;
    t.pitchError = r.pitchErrorDeg;
    t.pwmAngle = r.pwmAngleDeg;
    t.gpsHasFix = r.gps.hasFix;
    t.gpsLatitude = r.gps.latitudeDeg;
    t.gpsLongitude = r.gps.longitudeDeg;
    t.gpsAltitude = r.gps.altitudeM;
    t.gpsSpeed = r.gps.speedKmh * kKmhToMps;
    return t;
}

#endif // TELEMETRY_H
