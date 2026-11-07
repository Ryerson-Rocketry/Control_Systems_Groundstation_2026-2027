#include "telemetryframe.h"

#include <cmath>
#include <limits>
#include <type_traits>

namespace {

// The single definition of every column: name, decoded unit, scale factor,
// member. Order is the rocket's CSV column order.
template <typename Telemetry, typename Visitor>
void visitFields(Telemetry &data, Visitor &&visit)
{
    visit("timestamp", "ms", 1, data.timestampMs);
    visit("flight_state", "enum", 1, data.flightState);
    visit("accel_x", "mg", 100, data.accelMg.x);
    visit("accel_y", "mg", 100, data.accelMg.y);
    visit("accel_z", "mg", 100, data.accelMg.z);
    visit("gyro_x", "deg/s", 100, data.gyroDps.x);
    visit("gyro_y", "deg/s", 100, data.gyroDps.y);
    visit("gyro_z", "deg/s", 100, data.gyroDps.z);
    visit("mag_x", "mag units", 100, data.magnetometer.x);
    visit("mag_y", "mag units", 100, data.magnetometer.y);
    visit("mag_z", "mag units", 100, data.magnetometer.z);
    visit("roll", "deg", 100, data.rollDeg);
    visit("pitch", "deg", 100, data.pitchDeg);
    visit("yaw", "deg", 100, data.yawDeg);
    visit("tilt_angle", "deg", 100, data.tiltAngleDeg);
    visit("altitude", "m", 100, data.altitudeM);
    visit("velocity", "m/s", 100, data.velocityMps);
    visit("pressure", "Pa", 100, data.pressurePa);
    visit("temperature", "degC", 100, data.temperatureC);
    visit("roll_error", "deg", 100, data.rollErrorDeg);
    visit("pitch_error", "deg", 100, data.pitchErrorDeg);
    visit("pwm_angle", "deg", 100, data.pwmAngleDeg);
    visit("gps_has_fix", "bool", 1, data.gps.hasFix);
    visit("gps_latitude", "deg", 10000000, data.gps.latitudeDeg);
    visit("gps_longitude", "deg", 10000000, data.gps.longitudeDeg);
    visit("gps_altitude", "m ASL", 100, data.gps.altitudeM);
    visit("gps_speed", "km/h", 100, data.gps.speedKmh);
}

}

QString flightStateName(qint32 state)
{
    switch (state) {
    case StateIdle:
        return QStringLiteral("IDLE");
    case StateBoosterBurn:
        return QStringLiteral("BOOSTER BURN");
    case StateCanardActivation:
        return QStringLiteral("CANARD ACTIVATION");
    case StateApogee:
        return QStringLiteral("APOGEE");
    }
    return QStringLiteral("UNKNOWN (%1)").arg(state);
}

QList<TelemetryColumn> telemetryColumns()
{
    QList<TelemetryColumn> columns;
    const RocketTelemetry data;
    visitFields(data, [&columns](const char *name, const char *unit, int scale, const auto &) {
        columns.append({QByteArray(name), QByteArray(unit), scale});
    });
    return columns;
}

QList<double> telemetryValues(const TelemetryFrame &frame)
{
    QList<double> values;
    visitFields(frame.data, [&values](const char *, const char *, int, const auto &field) {
        values.append(static_cast<double>(field));
    });
    return values;
}

bool setTelemetryValues(const QList<double> &values, TelemetryFrame *frame)
{
    if (values.size() != telemetryColumns().size())
        return false;

    RocketTelemetry data;
    bool ok = true;
    qsizetype index = 0;

    visitFields(data, [&](const char *, const char *, int, auto &field) {
        using Field = std::decay_t<decltype(field)>;
        const double value = values[index++];
        if (!std::isfinite(value)) {
            ok = false;
            return;
        }
        if constexpr (std::is_integral_v<Field>) {
            const double min = static_cast<double>(std::numeric_limits<Field>::min());
            const double max = static_cast<double>(std::numeric_limits<Field>::max());
            if (value < min || value > max || value != std::floor(value)) {
                ok = false;
                return;
            }
        }
        field = static_cast<Field>(value);
    });

    if (ok)
        frame->data = data;
    return ok;
}
