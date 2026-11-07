#ifndef TELEMETRYFRAME_H
#define TELEMETRYFRAME_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

/*
 * telemetryframe.h
 * Purpose:
 *   One decoded line of rocket telemetry, in real units (the radio sends
 *   scaled integers; the parser divides them back out). Everything after
 *   the parser (hub, logger, UDP, UI) works with this struct.
 *
 * To add or change a field: change the struct here, then its one entry
 * in telemetryframe.cpp. The wire columns, CSV log, and UDP datagrams
 * all come from that list. The column order must match the rocket's.
 */

// Same values and order as the rocket firmware's FlightState_t.
enum FlightState : qint32
{
    StateIdle = 0,
    StateBoosterBurn = 1,
    StateCanardActivation = 2,
    StateApogee = 3,
    StateDescent = 4,
};

// Display name, e.g. "BOOSTER BURN"; unknown values show their number.
QString flightStateName(qint32 state);

struct Vector3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct GpsFix
{
    bool hasFix = false;        // position fields are meaningless without a fix
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    double altitudeM = 0.0;     // above sea level, unlike RocketTelemetry::altitudeM
    double speedKmh = 0.0;
};

struct RocketTelemetry
{
    quint32 timestampMs = 0;
    qint32 flightState = StateIdle;  // a FlightState; kept as int so new states still decode
    Vector3 accelMg;
    Vector3 gyroDps;
    Vector3 magnetometer;       // magnetometer units as stored
    double rollDeg = 0.0;
    double pitchDeg = 0.0;
    double yawDeg = 0.0;
    double tiltAngleDeg = 0.0;
    double altitudeM = 0.0;
    double velocityMps = 0.0;
    double pressurePa = 0.0;
    double temperatureC = 0.0;
    double rollErrorDeg = 0.0;
    double pitchErrorDeg = 0.0;
    double pwmAngleDeg = 0.0;
    GpsFix gps;
};

struct TelemetryFrame
{
    RocketTelemetry data;
    QDateTime receivedAtUtc;
};

Q_DECLARE_METATYPE(TelemetryFrame)

struct TelemetryColumn
{
    QByteArray name;        // e.g. "accel_x"
    QByteArray unit;        // decoded unit, e.g. "mg"
    int scale;              // wire value = decoded value * scale
};

// The rocket's CSV columns, in order; values line up with these columns.
QList<TelemetryColumn> telemetryColumns();
QList<double> telemetryValues(const TelemetryFrame &frame);

// Takes decoded values. Fails on a wrong count, non-finite values, or an
// integer field (timestamp, flight state) that is not a valid integer.
bool setTelemetryValues(const QList<double> &values, TelemetryFrame *frame);

#endif // TELEMETRYFRAME_H
