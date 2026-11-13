#ifndef TELEMETRYFRAME_H
#define TELEMETRYFRAME_H

#include <cmath>

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
 *
 * The radio packet is shorter than the full row, so a frame can lack
 * columns. A double the rocket did not send is NaN (see hasValue());
 * the timestamp, flight state and GPS fix flag are in every packet.
 */

// Same values and order as FlightState_t in the rocket firmware
// (Core/Inc/Defs/states.h). The rocket has two stages.
enum FlightState : qint32
{
    StateIdle = 0,                // STATE_PAD
    StateBoosterBurn = 1,         // STATE_BOOSTER_BURNOUT: liftoff until the booster is spent
    StateSeparation = 2,          // STATE_SEPARATION
    StateSustainerIgnition = 3,   // STATE_SUSTAINER_IGNITION: the second stage's burn
    StateCanardActivation = 4,    // STATE_CANARDS_ACTIVATE
    StateDescent = 5,             // STATE_DESCENT
};

// Display name, e.g. "BOOSTER BURN"; unknown values show their number.
QString flightStateName(qint32 state);

// False for a value the rocket's packet did not carry.
inline bool hasValue(double value) { return !std::isnan(value); }

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

// Takes decoded values, NaN for a column that was not sent. Fails on a
// wrong count, an infinite value, or an integer or bool field (timestamp,
// flight state, GPS fix) that is missing or not a valid integer.
bool setTelemetryValues(const QList<double> &values, TelemetryFrame *frame);

// The CSV lines the rocket sends. Each carries a subset of the columns, as
// scaled integers in column order.
enum class LineFormat
{
    // Every column: the firmware's SD-card row. Also what the simulator and
    // the wired sources send.
    FullRow,
    // What the firmware's radio task sends once a second (Radio_BuildPacket
    // in Core/Src/Tasks/radio.c): cut down to fit the air time.
    RadioPacket,
    // The radio packet plus what the dashboard shows: attitude, altitude,
    // velocity, pressure and temperature. The firmware does not send this
    // yet; it is here so the dashboard fills in as soon as it does.
    RadioPacketExtended,
};

// The columns a line of this format carries, in order, as indexes into
// telemetryColumns(). The parser tells formats apart by counting fields,
// so no two may have the same length.
QList<int> lineColumns(LineFormat format);
QList<LineFormat> lineFormats();

#endif // TELEMETRYFRAME_H
