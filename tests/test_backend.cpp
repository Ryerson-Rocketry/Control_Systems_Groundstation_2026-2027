#include <array>
#include <memory>

#include <fcntl.h>
#include <unistd.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include "groundstationbackend.h"
#include "i2csource.h"
#include "lorasource.h"
#include "packetparser.h"
#include "simsource.h"
#include "sx127x.h"
#include "telemetry.h"
#include "telemetrylogger.h"
#include "udptransport.h"

namespace {
// Stand-in for the G0 GPIO line: a pipe that becomes readable on edge().
class FakeDio0 : public InterruptLine
{
public:
    ~FakeDio0() override { close(); }

    bool open(QString *) override
    {
        if (m_fds[0] >= 0)
            return true;
        if (::pipe2(m_fds, O_NONBLOCK | O_CLOEXEC) != 0)
            return false;
        ++opens;
        return true;
    }
    void close() override
    {
        for (int &fd : m_fds) {
            if (fd >= 0)
                ::close(fd);
            fd = -1;
        }
    }
    int fd() const override { return m_fds[0]; }
    void clearEdges() override
    {
        char buffer[64];
        while (m_fds[0] >= 0 && ::read(m_fds[0], buffer, sizeof(buffer)) > 0) {
        }
    }

    void edge() { QVERIFY(::write(m_fds[1], "e", 1) == 1); }
    bool isOpen() const { return m_fds[0] >= 0; }
    int opens = 0;

private:
    int m_fds[2] = {-1, -1};
};

// Register-level stand-in for the SX127x chip, enough for the driver.
class FakeRadio : public SpiBus
{
public:
    FakeRadio() { regs[0x42] = 0x12; }

    bool open(QString *) override { return true; }
    void close() override {}

    bool transfer(QByteArray *data) override
    {
        if (failTransfers)
            return false;
        quint8 reg = quint8(data->at(0)) & 0x7F;
        const bool write = quint8(data->at(0)) & 0x80;
        for (qsizetype index = 1; index < data->size(); ++index) {
            if (write)
                writeRegister(reg, quint8(data->at(index)));
            else
                (*data)[index] = char(readRegister(reg));
            if (reg != 0x00) // FIFO bursts stay on the FIFO register
                ++reg;
        }
        return true;
    }

    void receive(const QByteArray &payload, quint8 rssiRegister = 100, qint8 snrRegister = 20)
    {
        const quint8 base = 0x40;
        for (qsizetype index = 0; index < payload.size(); ++index)
            fifo[quint8(base + index)] = quint8(payload[index]);
        regs[0x10] = base;
        regs[0x13] = quint8(payload.size());
        regs[0x1A] = rssiRegister;
        regs[0x19] = quint8(snrRegister);
        regs[0x12] |= 0x40;
    }

    std::array<quint8, 256> regs{};
    std::array<quint8, 256> fifo{};
    bool failTransfers = false;

private:
    void writeRegister(quint8 reg, quint8 value)
    {
        if (reg == 0x00)
            fifo[regs[0x0D]++] = value;
        else if (reg == 0x12)
            regs[0x12] &= quint8(~value); // IRQ flags clear on writing 1
        else
            regs[reg] = value;
    }

    quint8 readRegister(quint8 reg)
    {
        return reg == 0x00 ? fifo[regs[0x0D]++] : regs[reg];
    }
};

// Stand-in for the radio board's microcontroller, speaking I2cSource's
// status/packet register protocol.
class FakeRadioBoard : public I2cBus
{
public:
    bool open(QString *) override { return true; }
    void close() override {}

    bool readRegister(quint8 reg, QByteArray *data) override
    {
        if (failReads)
            return false;
        largestRead = qMax(largestRead, data->size());

        if (reg == I2cSource::kStatusRegister) {
            const int length = queue.isEmpty() ? 0 : int(queue.first().size());
            (*data)[0] = char(length);
            (*data)[1] = char(rssiDbm & 0xFF);
            (*data)[2] = char((rssiDbm >> 8) & 0xFF);
            (*data)[3] = char(snrQuarterDb);
            return true;
        }
        if (reg == I2cSource::kPacketRegister && !queue.isEmpty()) {
            *data = queue.first().mid(offset, data->size());
            offset += data->size();
            if (offset >= queue.first().size()) {
                queue.removeFirst();
                offset = 0;
            }
            return true;
        }
        return false;
    }

    QList<QByteArray> queue;
    qint16 rssiDbm = -72;
    qint8 snrQuarterDb = 34;
    bool failReads = false;
    qsizetype largestRead = 0;

private:
    qsizetype offset = 0;
};

const QByteArray kRadioHeadBroadcast("\xFF\xFF\x00\x00", 4);

// The full row as the rocket writes it to its SD card: scaled integers,
// all 27 columns.
const QByteArray kRocketLine =
    "123456,2,-1250,3400,98100,1525,-250,9000,2050,-3025,4575,"
    "4500,-150,275,312,15230,-842,9990050,-325,120,-80,-480,"
    "1,436577120,-793788450,35230,3031";

// Lines exactly as Radio_BuildPacket() in the rocket firmware formats them
// (Core/Src/Tasks/radio.c): the callsign, then 14 scaled integers.
const QByteArray kRadioLineOnPad = "VE3HIV,15000,0,-1250,3400,99870,0,0,0,0,0,0,0,0,0";
const QByteArray kRadioLineInFlight =
    "VE3HIV,1834567,4,-25030,31090,523010,1250,-825,34020,-675,1,426928123,-771906456,215039,31060";
const QByteArray kRadioLineLongest =
    "VE3HIV,4294967295,5,-1600000,-1600000,-1600000,-200000,-200000,-200000,-1000,"
    "1,-899999999,-1799999999,3000000,999900";

// The same moment from Radio_BuildPacket() with the dashboard's fields
// added (22 integers): what LineFormat::RadioPacketExtended decodes.
const QByteArray kRadioLineExtended =
    "VE3HIV,1834567,4,-25030,31090,523010,1250,-825,34020,250,-375,-16125,450,198725,21250,"
    "7950150,625,-675,1,426928123,-771906456,215039,31060";

RocketTelemetry radioSample()
{
    PacketParser parser;
    const QList<TelemetryFrame> frames = parser.feed(kRadioLineInFlight + '\n');
    return frames.isEmpty() ? RocketTelemetry{} : frames.first().data;
}

// What kRadioLineInFlight carries, and nothing else.
void verifyRadioSample(const RocketTelemetry &data)
{
    QCOMPARE(data.timestampMs, 1834567u);
    QCOMPARE(data.flightState, qint32(StateCanardActivation));
    QCOMPARE(data.accelMg.x, -250.3);
    QCOMPARE(data.accelMg.y, 310.9);
    QCOMPARE(data.accelMg.z, 5230.1);
    QCOMPARE(data.gyroDps.x, 12.5);
    QCOMPARE(data.gyroDps.y, -8.25);
    QCOMPARE(data.gyroDps.z, 340.2);
    QCOMPARE(data.pwmAngleDeg, -6.75);
    QCOMPARE(data.gps.hasFix, true);
    QCOMPARE(data.gps.latitudeDeg, 42.6928123);
    QCOMPARE(data.gps.longitudeDeg, -77.1906456);
    QCOMPARE(data.gps.altitudeM, 2150.39);
    QCOMPARE(data.gps.speedKmh, 310.6);

    for (const double notSent : {data.magnetometer.x, data.magnetometer.y, data.magnetometer.z,
                                 data.rollDeg, data.pitchDeg, data.yawDeg, data.tiltAngleDeg,
                                 data.altitudeM, data.velocityMps, data.pressurePa, data.temperatureC,
                                 data.rollErrorDeg, data.pitchErrorDeg}) {
        QVERIFY(!hasValue(notSent));
    }
}

const QByteArray kRocketHeader =
    "timestamp (ms),flight_state (enum),accel_x (centi-mg),accel_y (centi-mg),"
    "accel_z (centi-mg),gyro_x (centi-dps),gyro_y (centi-dps),gyro_z (centi-dps),"
    "mag_x (scaled x100),mag_y (scaled x100),mag_z (scaled x100),roll (centi-deg),"
    "pitch (centi-deg),yaw (centi-deg),tilt_angle (centi-deg),altitude (cm),"
    "velocity (cm/s),pressure (centi-Pa),temperature (centi-degC),"
    "roll_error (centi-deg),pitch_error (centi-deg),pwm_angle (centi-deg),"
    "gps_has_fix (bool),gps_latitude (decimal deg),gps_longitude (decimal deg),"
    "gps_altitude (m ASL),gps_speed (km/h)";

TelemetryFrame sampleFrame()
{
    PacketParser parser;
    const QList<TelemetryFrame> frames = parser.feed(kRocketLine + '\n');
    return frames.isEmpty() ? TelemetryFrame{} : frames.first();
}

void verifySample(const RocketTelemetry &data)
{
    QCOMPARE(data.timestampMs, 123456u);
    QCOMPARE(data.flightState, 2);
    QCOMPARE(data.accelMg.x, -12.5);
    QCOMPARE(data.accelMg.y, 34.0);
    QCOMPARE(data.accelMg.z, 981.0);
    QCOMPARE(data.gyroDps.x, 15.25);
    QCOMPARE(data.gyroDps.y, -2.5);
    QCOMPARE(data.gyroDps.z, 90.0);
    QCOMPARE(data.magnetometer.x, 20.5);
    QCOMPARE(data.magnetometer.y, -30.25);
    QCOMPARE(data.magnetometer.z, 45.75);
    QCOMPARE(data.rollDeg, 45.0);
    QCOMPARE(data.pitchDeg, -1.5);
    QCOMPARE(data.yawDeg, 2.75);
    QCOMPARE(data.tiltAngleDeg, 3.12);
    QCOMPARE(data.altitudeM, 152.3);
    QCOMPARE(data.velocityMps, -8.42);
    QCOMPARE(data.pressurePa, 99900.5);
    QCOMPARE(data.temperatureC, -3.25);
    QCOMPARE(data.rollErrorDeg, 1.2);
    QCOMPARE(data.pitchErrorDeg, -0.8);
    QCOMPARE(data.pwmAngleDeg, -4.8);
    QCOMPARE(data.gps.hasFix, true);
    QCOMPARE(data.gps.latitudeDeg, 43.657712);
    QCOMPARE(data.gps.longitudeDeg, -79.378845);
    QCOMPARE(data.gps.altitudeM, 352.3);
    QCOMPARE(data.gps.speedKmh, 30.31);
}

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class TestBackend : public QObject
{
    Q_OBJECT

private slots:
    void parserDecodesRocketLine()
    {
        PacketParser parser;
        const auto frames = parser.feed(kRocketLine + '\n');
        QCOMPARE(frames.size(), 1);
        verifySample(frames[0].data);
        QVERIFY(frames[0].receivedAtUtc.isValid());
        QCOMPARE(parser.rejectedCount(), quint64(0));
    }

    void parserMatchesRocketColumnOrder()
    {
        // Same order as the rocket's header labels, so the table can't drift.
        const QList<QByteArray> headers = kRocketHeader.split(',');
        const QList<TelemetryColumn> columns = telemetryColumns();
        QCOMPARE(columns.size(), headers.size());
        for (qsizetype index = 0; index < columns.size(); ++index)
            QVERIFY2(headers[index].startsWith(columns[index].name + ' '), headers[index].constData());
    }

    void parserSkipsHeaderAndHandlesCrlf()
    {
        PacketParser parser;
        const auto frames = parser.feed(kRocketHeader + "\r\n" + kRocketLine + "\r\n");
        QCOMPARE(frames.size(), 1);
        verifySample(frames[0].data);
        QCOMPARE(parser.rejectedCount(), quint64(0));
    }

    void encodeReproducesRocketLine()
    {
        QCOMPARE(PacketParser::encode(sampleFrame()), kRocketLine + '\n');
    }

    void parserRejectsBadLines()
    {
        QList<QByteArray> fields = kRocketLine.split(',');
        const auto withField = [&fields](int index, const QByteArray &value) {
            QList<QByteArray> changed = fields;
            changed[index] = value;
            return changed.join(',') + '\n';
        };

        PacketParser parser;
        QVERIFY(parser.feed(fields.mid(1).join(',') + '\n').isEmpty());     // column missing
        QVERIFY(parser.feed(kRocketLine + ",7\n").isEmpty());               // extra column
        QVERIFY(parser.feed(withField(5, "15.25")).isEmpty());              // not an integer
        QVERIFY(parser.feed(withField(5, "abc")).isEmpty());
        QVERIFY(parser.feed(withField(5, "")).isEmpty());
        QVERIFY(parser.feed(withField(0, "-1")).isEmpty());                 // negative timestamp
        QVERIFY(parser.feed(withField(1, "3000000000")).isEmpty());         // flight_state > int32
        QVERIFY(parser.feed(withField(2, "99999999999")).isEmpty());        // beyond 32 bits
        QVERIFY(parser.feed(withField(22, "2")).isEmpty());                // gps_has_fix not 0/1
        QVERIFY(parser.feed("garbage\n").isEmpty());
        QCOMPARE(parser.rejectedCount(), quint64(10));

        // A good line after all that still decodes.
        QCOMPARE(parser.feed(kRocketLine + '\n').size(), 1);
    }

    void parserDecodesRadioPacket()
    {
        PacketParser parser;
        const auto flight = parser.feed(kRadioLineInFlight + '\n');
        QCOMPARE(flight.size(), 1);
        verifyRadioSample(flight[0].data);
        QVERIFY(flight[0].receivedAtUtc.isValid());

        // On the pad with no GPS fix: the shortest line the rocket sends
        const auto pad = parser.feed(kRadioLineOnPad + '\n');
        QCOMPARE(pad.size(), 1);
        QCOMPARE(pad[0].data.timestampMs, 15000u);
        QCOMPARE(pad[0].data.flightState, qint32(StateIdle));
        QCOMPARE(pad[0].data.accelMg.z, 998.7);
        QCOMPARE(pad[0].data.gps.hasFix, false);
        QCOMPARE(pad[0].data.gps.latitudeDeg, 0.0);

        // Every field at its widest
        const auto longest = parser.feed(kRadioLineLongest + '\n');
        QCOMPARE(longest.size(), 1);
        QCOMPARE(longest[0].data.timestampMs, 4294967295u);
        QCOMPARE(longest[0].data.flightState, qint32(StateDescent));
        QCOMPARE(longest[0].data.gps.longitudeDeg, -179.9999999);

        QCOMPARE(parser.rejectedCount(), quint64(0));
    }

    void parserDecodesExtendedRadioPacket()
    {
        PacketParser parser;
        const auto frames = parser.feed(kRadioLineExtended + '\n');
        QCOMPARE(frames.size(), 1);
        QCOMPARE(parser.rejectedCount(), quint64(0));

        const RocketTelemetry &data = frames[0].data;
        QCOMPARE(data.timestampMs, 1834567u);
        QCOMPARE(data.flightState, qint32(StateCanardActivation));
        QCOMPARE(data.accelMg.z, 5230.1);
        QCOMPARE(data.gyroDps.z, 340.2);
        QCOMPARE(data.rollDeg, 2.5);
        QCOMPARE(data.pitchDeg, -3.75);
        QCOMPARE(data.yawDeg, -161.25);
        QCOMPARE(data.tiltAngleDeg, 4.5);
        QCOMPARE(data.altitudeM, 1987.25);
        QCOMPARE(data.velocityMps, 212.5);
        QCOMPARE(data.pressurePa, 79501.5);
        QCOMPARE(data.temperatureC, 6.25);
        QCOMPARE(data.pwmAngleDeg, -6.75);
        QCOMPARE(data.gps.hasFix, true);
        QCOMPARE(data.gps.latitudeDeg, 42.6928123);
        QCOMPARE(data.gps.speedKmh, 310.6);

        for (const double notSent : {data.magnetometer.x, data.magnetometer.y, data.magnetometer.z,
                                     data.rollErrorDeg, data.pitchErrorDeg}) {
            QVERIFY(!hasValue(notSent));
        }
    }

    void parserAcceptsAnyCallsignOrNone()
    {
        const QByteArray numbers = kRadioLineInFlight.mid(kRadioLineInFlight.indexOf(',') + 1);
        const QList<QByteArray> lines = {numbers, "VA3ABC/P," + numbers, "ve3hiv," + numbers,
                                         " VE3HIV ," + numbers};
        PacketParser parser;
        for (const QByteArray &line : lines) {
            const auto frames = parser.feed(line + '\n');
            QVERIFY2(frames.size() == 1, line.constData());
            verifyRadioSample(frames[0].data);
        }

        // The full row may carry one too
        const auto full = parser.feed("VE3HIV," + kRocketLine + '\n');
        QCOMPARE(full.size(), 1);
        verifySample(full[0].data);
        QCOMPARE(parser.rejectedCount(), quint64(0));
    }

    void parserRejectsMalformedRadioPackets()
    {
        const QList<QByteArray> fields = kRadioLineInFlight.split(',');
        const auto withField = [&fields](int index, const QByteArray &value) {
            QList<QByteArray> changed = fields;
            changed[index] = value;
            return changed.join(',') + '\n';
        };

        PacketParser parser;
        QVERIFY(parser.feed(fields.mid(0, fields.size() - 1).join(',') + '\n').isEmpty()); // field missing
        QVERIFY(parser.feed(kRadioLineInFlight + ",7\n").isEmpty());                         // extra field
        QVERIFY(parser.feed("VE3HIV\n").isEmpty());                                          // callsign only
        QVERIFY(parser.feed("VE3HIV," + kRadioLineInFlight + '\n').isEmpty());               // two callsigns
        QVERIFY(parser.feed("CALLSIGNTOOLONG" + kRadioLineInFlight.mid(6) + '\n').isEmpty());
        QVERIFY(parser.feed(withField(1, "")).isEmpty());                                    // no timestamp
        QVERIFY(parser.feed(withField(3, "1.5")).isEmpty());                                 // not an integer
        QVERIFY(parser.feed(withField(10, "2")).isEmpty());                                  // gps_has_fix not 0/1
        QCOMPARE(parser.rejectedCount(), quint64(8));

        QCOMPARE(parser.feed(kRadioLineInFlight + '\n').size(), 1);
    }

    void lineFormatsAreToldApartByLength()
    {
        const QList<TelemetryColumn> columns = telemetryColumns();
        QCOMPARE(lineColumns(LineFormat::FullRow).size(), 27);
        QCOMPARE(lineColumns(LineFormat::RadioPacket).size(), 14);
        QCOMPARE(lineColumns(LineFormat::RadioPacketExtended).size(), 22);

        QSet<qsizetype> lengths;
        for (const LineFormat format : lineFormats()) {
            const QList<int> layout = lineColumns(format);
            QVERIFY(!lengths.contains(layout.size()));
            lengths.insert(layout.size());
            QCOMPARE(QSet<int>(layout.begin(), layout.end()).size(), layout.size()); // no column twice

            // Integer and bool fields can't be marked as not sent, so every line needs them
            for (int index = 0; index < columns.size(); ++index) {
                if (columns[index].scale == 1)
                    QVERIFY2(layout.contains(index), columns[index].name.constData());
            }
        }
    }

    void encodeReproducesRadioPackets()
    {
        // Same bytes as the firmware's snprintf(), so the column lists can't
        // drift from it unnoticed.
        PacketParser parser;
        const auto frames = parser.feed(kRadioLineInFlight + '\n' + kRadioLineExtended + '\n');
        QCOMPARE(frames.size(), 2);
        QCOMPARE(PacketParser::encode(frames[0], LineFormat::RadioPacket, "VE3HIV"), kRadioLineInFlight + '\n');
        QCOMPARE(PacketParser::encode(frames[1], LineFormat::RadioPacketExtended, "VE3HIV"),
                 kRadioLineExtended + '\n');

        // A column the frame lacks goes out as 0
        const QList<QByteArray> full = PacketParser::encode(frames[0]).trimmed().split(',');
        QCOMPARE(full.size(), 27);
        QCOMPARE(full[15], QByteArray("0")); // altitude
    }

    void dashboardShowsWhatTheRadioPacketLacks()
    {
        TelemetryFrame frame;
        frame.data = radioSample();
        Telemetry t = telemetryFromFrame(frame);

        QCOMPARE(t.missionTime, 1834.567);
        QCOMPARE(t.flightState, qint32(StateCanardActivation));
        QVERIFY(qAbs(t.gpsSpeed - 310.6 / 3.6) < 1e-9);
        QCOMPARE(formatReading(t.gpsAltitude, 1), QStringLiteral("2150.4"));
        QCOMPARE(formatReading(-0.3, 0), QStringLiteral("0"));
        QCOMPARE(formatReading(-161.25, 0), QStringLiteral("-161"));
        for (const double notSent : {t.altitude, t.velocity, t.pressure, t.temperature, t.roll, t.pitch, t.yaw,
                                     t.tiltAngle}) {
            QVERIFY(!hasValue(notSent));
            QCOMPARE(formatReading(notSent, 1), QStringLiteral("--"));
        }

        // Without altitude the flight state is the only evidence of where
        // the rocket is, and it never says "landed".
        QVERIFY(!t.hasAltitude());
        QVERIFY(!t.onGround());
        QVERIFY(!t.landed());
        t.flightState = StateIdle;
        QVERIFY(t.onGround());
        t.flightState = StateDescent;
        QVERIFY(!t.onGround());
        QVERIFY(!t.landed());

        // With it, landing is Descent at rest on the ground
        t.altitude = 0.4;
        t.velocity = -0.2;
        QVERIFY(t.hasAltitude());
        QVERIFY(t.landed());
        t.altitude = 310.0;
        QVERIFY(!t.onGround());
        QVERIFY(!t.landed());
    }

    void parserHandlesSplitChunks()
    {
        PacketParser parser;
        const QByteArray line = kRocketLine + '\n';
        int frames = 0;
        for (const char byte : line)
            frames += parser.feed(QByteArray(1, byte)).size();
        QCOMPARE(frames, 1);
    }

    void parserDiscardsEndlessNoise()
    {
        PacketParser parser;
        QVERIFY(parser.feed(QByteArray(4096, 'x')).isEmpty());
        QCOMPARE(parser.rejectedCount(), quint64(1));
        QCOMPARE(parser.feed(kRocketLine + '\n').size(), 1);
    }

    void simulatorOutputPassesParser()
    {
        SimSource source(1);
        PacketParser parser;
        QList<TelemetryFrame> frames;
        connect(&source, &TelemetrySource::bytesReceived, this,
                [&](const QByteArray &bytes) { frames += parser.feed(bytes); });
        source.start();
        QTRY_VERIFY(frames.size() >= 20);
        source.stop();

        QCOMPARE(parser.rejectedCount(), quint64(0));
        QVERIFY(frames.last().data.timestampMs > frames.first().data.timestampMs);
        QVERIFY(frames.last().data.accelMg.z > 0.0);
        QVERIFY(frames.last().data.pressurePa > 100000.0);
    }

    void simulatorGetsGpsFixAfterStartup()
    {
        QVERIFY(!SimSource::frameAt(0).data.gps.hasFix);
        const GpsFix fix = SimSource::frameAt(30000).data.gps;
        QVERIFY(fix.hasFix);
        QVERIFY(fix.latitudeDeg > 42.0 && fix.latitudeDeg < 43.0);
        QVERIFY(fix.longitudeDeg < -77.0 && fix.longitudeDeg > -78.0);
        QVERIFY(fix.altitudeM > 200.0);

        // Survives the trip through the wire format with full precision.
        PacketParser parser;
        const auto frames = parser.feed(PacketParser::encode(SimSource::frameAt(30000)));
        QCOMPARE(frames.size(), 1);
        QVERIFY(qAbs(frames[0].data.gps.latitudeDeg - fix.latitudeDeg) < 1e-7);
    }

    void flightStatesMatchFirmwareEnum()
    {
        // STATE_PAD .. STATE_DESCENT in the firmware's Core/Inc/Defs/states.h
        QCOMPARE(flightStateName(0), QStringLiteral("IDLE"));
        QCOMPARE(flightStateName(1), QStringLiteral("BOOSTER BURN"));
        QCOMPARE(flightStateName(2), QStringLiteral("SEPARATION"));
        QCOMPARE(flightStateName(3), QStringLiteral("SUSTAINER IGNITION"));
        QCOMPARE(flightStateName(4), QStringLiteral("CANARD ACTIVATION"));
        QCOMPARE(flightStateName(5), QStringLiteral("DESCENT"));
        QCOMPARE(flightStateName(9), QStringLiteral("UNKNOWN (9)"));
        QCOMPARE(kStateCount, 6);
        QCOMPARE(qint32(StateDescent), kStateCount - 1);
    }

    void simulatorFliesEveryStateInOrder()
    {
        QList<qint32> states;
        for (quint32 ms = 0; ms < 140000; ms += 100) {
            const RocketTelemetry data = SimSource::frameAt(ms).data;
            if (states.isEmpty() || states.last() != data.flightState)
                states.append(data.flightState);

            // Canards only move while they are active.
            if (data.flightState != StateCanardActivation)
                QCOMPARE(data.pwmAngleDeg, 0.0);
        }
        QCOMPARE(states, (QList<qint32>{StateIdle, StateBoosterBurn, StateSeparation, StateSustainerIgnition,
                                        StateCanardActivation, StateDescent}));
    }

    void datagramRoundTrips()
    {
        const TelemetryFrame original = sampleFrame();
        TelemetryFrame decoded;
        QVERIFY(decodeDatagram(encodeDatagram(original), &decoded));
        verifySample(decoded.data);
        QCOMPARE(decoded.receivedAtUtc.toMSecsSinceEpoch(), original.receivedAtUtc.toMSecsSinceEpoch());
    }

    void datagramRejectsBadInput()
    {
        TelemetryFrame frame;
        QVERIFY(!decodeDatagram("not json", &frame));
        QVERIFY(!decodeDatagram("{}", &frame));

        // A missing field must fail, not silently become zero.
        QJsonObject root = QJsonDocument::fromJson(encodeDatagram(sampleFrame())).object();
        QJsonObject values = root["values"].toObject();
        values.remove("altitude");
        root["values"] = values;
        QVERIFY(!decodeDatagram(QJsonDocument(root).toJson(), &frame));
    }

    void datagramCarriesMissingColumnsAsNull()
    {
        TelemetryFrame original;
        original.data = radioSample();
        original.receivedAtUtc = QDateTime::currentDateTimeUtc();

        const QByteArray datagram = encodeDatagram(original);
        QJsonObject root = QJsonDocument::fromJson(datagram).object();
        QJsonObject values = root["values"].toObject();
        QVERIFY(values.contains("altitude"));
        QVERIFY(values["altitude"].isNull());
        QCOMPARE(values["gps_altitude"].toDouble(), 2150.39);

        TelemetryFrame decoded;
        QVERIFY(decodeDatagram(datagram, &decoded));
        verifyRadioSample(decoded.data);

        // A field every packet carries can't be null
        values["flight_state"] = QJsonValue(QJsonValue::Null);
        root["values"] = values;
        QVERIFY(!decodeDatagram(QJsonDocument(root).toJson(), &decoded));
    }

    void udpDeliversFramesOnLoopbackOnly()
    {
        UdpTelemetryReceiver receiver;
        QVERIFY(receiver.start(0));
        QVERIFY(receiver.address().isLoopback());

        QSignalSpy frames(&receiver, &UdpTelemetryReceiver::frameReceived);
        UdpTelemetrySender sender(receiver.port());
        sender.send(sampleFrame());

        QTRY_COMPARE(frames.size(), 1);
        verifySample(frames[0][0].value<TelemetryFrame>().data);

        QUdpSocket stray;
        stray.writeDatagram("garbage", QHostAddress::LocalHost, receiver.port());
        QTRY_COMPARE(receiver.rejectedCount(), quint64(1));
    }

    void loggerWritesRealUnitsAndExports()
    {
        QTemporaryDir root;
        QTemporaryDir usb;
        TelemetryLogger logger(root.path());
        QVERIFY(logger.startSession());
        logger.log(sampleFrame());
        logger.log(sampleFrame());

        const QDir session(logger.sessionDir());
        const QList<QByteArray> lines = readAll(session.filePath("telemetry.csv")).split('\n');
        QCOMPARE(lines.size(), 4); // header, 2 rows, trailing empty
        QVERIFY(lines[0].startsWith("received_utc,timestamp (ms),flight_state (enum),"));
        QVERIFY(lines[0].contains(",altitude (m),"));
        QCOMPARE(lines[0].count(','), telemetryColumns().size());
        QCOMPARE(lines[1].count(','), telemetryColumns().size());
        QVERIFY(lines[1].contains(",123456,2,-12.50,"));
        QVERIFY(lines[1].contains(",152.30,"));

        // Exporting twice must replace the older copy, not fail.
        QString error;
        QVERIFY2(logger.exportTo(usb.path(), &error), qPrintable(error));
        logger.log(sampleFrame());
        QVERIFY2(logger.exportTo(usb.path(), &error), qPrintable(error));

        const QString copy = QDir(usb.path()).filePath(
            QStringLiteral("GroundstationLogs/%1/telemetry.csv").arg(session.dirName()));
        QCOMPARE(readAll(copy).count('\n'), 4);
    }

    void loggerLeavesMissingColumnsEmpty()
    {
        QTemporaryDir root;
        TelemetryLogger logger(root.path());
        QVERIFY(logger.startSession());

        TelemetryFrame frame;
        frame.data = radioSample();
        frame.receivedAtUtc = QDateTime::currentDateTimeUtc();
        logger.log(frame);

        const QList<QByteArray> lines = readAll(QDir(logger.sessionDir()).filePath("telemetry.csv")).split('\n');
        QCOMPARE(lines.size(), 3); // header, 1 row, trailing empty
        QVERIFY(!lines[1].contains("nan"));

        // Still one cell per column, after received_utc
        const QList<QByteArray> cells = lines[1].split(',');
        const QList<int> sent = lineColumns(LineFormat::RadioPacket);
        QCOMPARE(cells.size(), telemetryColumns().size() + 1);
        for (int index = 0; index < telemetryColumns().size(); ++index)
            QCOMPARE(cells[index + 1].isEmpty(), !sent.contains(index));
        QCOMPARE(cells[1], QByteArray("1834567"));
        QCOMPARE(cells[3], QByteArray("-250.30"));
    }

    void loggerSeparatesSessions()
    {
        QTemporaryDir root;
        TelemetryLogger first(root.path());
        TelemetryLogger second(root.path());
        QVERIFY(first.startSession());
        QVERIFY(second.startSession());
        QVERIFY(first.sessionDir() != second.sessionDir());
    }

    void backendRunsEndToEndInSimulation()
    {
        QTemporaryDir root;
        QTemporaryDir usb;
        BackendConfig config;
        config.input = TelemetryInput::Simulated;
        config.simIntervalMs = 5;
        config.logRoot = root.path();
        config.udpPort = 0;

        GroundstationBackend backend(config);
        QSignalSpy exports(&backend, &GroundstationBackend::exportFinished);
        backend.start();

        QTRY_VERIFY(backend.hub()->frameCount() >= 10);
        QCOMPARE(qsizetype(backend.hub()->frameCount()), backend.hub()->history().size());

        backend.exportLogs(usb.path());
        QTRY_COMPARE(exports.size(), 1);
        QVERIFY(exports[0][0].toBool());
        QVERIFY(QDir(usb.path()).exists("GroundstationLogs"));

        backend.stop();
        backend.stop(); // must be safe to call twice

        backend.exportLogs(usb.path());
        QCOMPARE(exports.size(), 2);
        QVERIFY(!exports[1][0].toBool());
    }

    void radioConfiguresForRocketSettings()
    {
        FakeRadio bus;
        Sx127x radio(&bus);
        QString error;
        QVERIFY2(radio.begin(LoRaSettings{}, &error), qPrintable(error));

        // 433 MHz * 2^19 / 32 MHz = 0x6C4000
        QCOMPARE(bus.regs[0x06], quint8(0x6C));
        QCOMPARE(bus.regs[0x07], quint8(0x40));
        QCOMPARE(bus.regs[0x08], quint8(0x00));
        QCOMPARE(bus.regs[0x1D], quint8(0x72)); // 125 kHz, CR 4/5, explicit header
        QCOMPARE(bus.regs[0x1E], quint8(0x94)); // SF9, CRC on
        QCOMPARE(bus.regs[0x26], quint8(0x04)); // AGC on, no low data rate optimize
        QCOMPARE(bus.regs[0x39], quint8(0x12)); // sync word
        QCOMPARE(bus.regs[0x21], quint8(8));    // preamble
        QCOMPARE(bus.regs[0x01], quint8(0x8D)); // LoRa, low band, RX continuous
    }

    void radioEnablesLowDataRateOptimizeForSlowSettings()
    {
        FakeRadio bus;
        Sx127x radio(&bus);
        LoRaSettings settings;
        settings.spreadingFactor = 12;
        QString error;
        QVERIFY2(radio.begin(settings, &error), qPrintable(error));
        QCOMPARE(bus.regs[0x26], quint8(0x0C));
    }

    void radioRejectsMissingChipAndBadSettings()
    {
        QString error;
        {
            FakeRadio bus;
            bus.regs[0x42] = 0x00;
            Sx127x radio(&bus);
            QVERIFY(!radio.begin(LoRaSettings{}, &error));
            QVERIFY(error.contains("0x00"));
        }
        {
            FakeRadio bus;
            bus.failTransfers = true;
            Sx127x radio(&bus);
            QVERIFY(!radio.begin(LoRaSettings{}, &error));
        }
        FakeRadio bus;
        Sx127x radio(&bus);
        LoRaSettings settings;
        settings.bandwidthHz = 100000;
        QVERIFY(!radio.begin(settings, &error));
        settings = {};
        settings.spreadingFactor = 13;
        QVERIFY(!radio.begin(settings, &error));
    }

    void radioReceivesPacketsAndFlagsCrcErrors()
    {
        FakeRadio bus;
        Sx127x radio(&bus);
        QString error;
        QVERIFY(radio.begin(LoRaSettings{}, &error));

        LoRaPacket packet;
        QCOMPARE(radio.poll(&packet), Sx127x::Event::None);

        bus.receive("hello", 100, 20);
        QCOMPARE(radio.poll(&packet), Sx127x::Event::PacketReceived);
        QCOMPARE(packet.payload, QByteArray("hello"));
        QCOMPARE(packet.rssiDbm, -64);
        QCOMPARE(packet.snrDb, 5.0);
        QCOMPARE(bus.regs[0x12], quint8(0));
        QCOMPARE(radio.poll(&packet), Sx127x::Event::None);

        // Weak signal: negative SNR lowers the reported RSSI.
        bus.receive("weak", 40, -20);
        QCOMPARE(radio.poll(&packet), Sx127x::Event::PacketReceived);
        QCOMPARE(packet.snrDb, -5.0);
        QCOMPARE(packet.rssiDbm, -164 + 40 - 5);

        bus.receive("bad");
        bus.regs[0x12] |= 0x20;
        QCOMPARE(radio.poll(&packet), Sx127x::Event::CrcError);
    }

    void radioNeverLeavesReceiveMode()
    {
        // Receive only: the chip must never be switched to transmit, so
        // no packet from the rocket is ever missed.
        FakeRadio bus;
        Sx127x radio(&bus);
        QString error;
        QVERIFY(radio.begin(LoRaSettings{}, &error));

        LoRaPacket packet;
        for (int index = 0; index < 5; ++index) {
            bus.receive(kRocketLine);
            QCOMPARE(radio.poll(&packet), Sx127x::Event::PacketReceived);
            QCOMPARE(packet.payload, kRocketLine);
            QCOMPARE(bus.regs[0x01], quint8(0x8D));
        }
        QCOMPARE(bus.regs[0x09], quint8(0)); // TX power never configured
    }

    void radioReportsFaultWhenChipResets()
    {
        FakeRadio bus;
        Sx127x radio(&bus);
        QString error;
        QVERIFY(radio.begin(LoRaSettings{}, &error));

        bus.regs[0x01] = 0x09; // power-on default: FSK standby
        LoRaPacket packet;
        QCOMPARE(radio.poll(&packet), Sx127x::Event::Fault);
    }

    void radioCarriesRealisticRocketLine()
    {
        // Every column at a plausible flight extreme still fits one packet.
        // (A line with every column at full int32 width would be over 255
        // bytes and could not be sent; the rocket must never produce one.)
        const QByteArray extreme =
            "4294967295,5,-1600000,-1600000,-1600000,-200000,-200000,-200000,"
            "-100000,-100000,-100000,-18000,-18000,-18000,18000,-300000,-30000,"
            "10132500,-4000,-18000,-18000,-18000,"
            "1,-900000000,-1800000000,900000,99999";
        PacketParser parser;
        QCOMPARE(parser.feed(extreme + '\n').size(), 1);
        QVERIFY(kRadioHeadBroadcast.size() + extreme.size() + 1 <= Sx127x::kMaxPayloadBytes);
    }

    void loraSourceStripsAdafruitHeader()
    {
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        config.dio0Line = -1;
        LoRaSource source(config, std::move(bus));

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        QSignalSpy quality(&source, &TelemetrySource::signalQuality);
        QSignalSpy corrupted(&source, &TelemetrySource::packetCorrupted);
        source.start();
        QCOMPARE(link.size(), 1);
        QVERIFY(link[0][0].toBool());

        fake->receive(kRadioHeadBroadcast + kRocketLine);
        QTRY_COMPARE(bytes.size(), 1);
        QCOMPARE(bytes[0][0].toByteArray(), kRocketLine + '\n');
        QCOMPARE(quality.size(), 1);
        QCOMPARE(quality[0][0].toInt(), -64);

        fake->receive("ab"); // shorter than the header
        QTRY_COMPARE(corrupted.size(), 1);
        QCOMPARE(bytes.size(), 1);
    }

    void simulatedFlightOverRadioReachesDashboardInSiUnits_data()
    {
        QTest::addColumn<int>("format");
        QTest::addColumn<QByteArray>("callsign");
        QTest::addColumn<bool>("carriesAltitude");
        QTest::newRow("radio packet as the firmware sends it")
            << int(LineFormat::RadioPacket) << QByteArray("VE3HIV") << false;
        QTest::newRow("radio packet with the dashboard's fields")
            << int(LineFormat::RadioPacketExtended) << QByteArray("VE3HIV") << true;
        QTest::newRow("full row") << int(LineFormat::FullRow) << QByteArray() << true;
    }

    // A whole simulated flight as raw LoRa packets: fake chip -> real driver
    // -> header strip -> parser -> the SI values the dashboard shows.
    void simulatedFlightOverRadioReachesDashboardInSiUnits()
    {
        QFETCH(int, format);
        QFETCH(QByteArray, callsign);
        QFETCH(bool, carriesAltitude);

        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        config.dio0Line = -1;
        LoRaSource source(config, std::move(bus));
        PacketParser parser;
        QList<Telemetry> received;
        connect(&source, &TelemetrySource::bytesReceived, this, [&](const QByteArray &bytes) {
            const QList<TelemetryFrame> frames = parser.feed(bytes);
            for (const TelemetryFrame &frame : frames)
                received.append(telemetryFromFrame(frame));
        });
        source.start();

        constexpr int kFlightMs = 130000;
        constexpr int kStepMs = 1000;
        for (int ms = 0; ms <= kFlightMs; ms += kStepMs) {
            const TelemetryFrame sent = SimSource::frameAt(quint32(ms));
            QByteArray line = PacketParser::encode(sent, static_cast<LineFormat>(format), callsign);
            line.chop(1);  // the radio packet is the line, without a newline
            QVERIFY2(kRadioHeadBroadcast.size() + line.size() <= 255, "packet too big for one LoRa frame");

            const qsizetype before = received.size();
            fake->receive(kRadioHeadBroadcast + line);
            QTRY_COMPARE(received.size(), before + 1);

            const Telemetry &t = received.last();
            QCOMPARE(t.missionTime, ms / 1000.0);
            QCOMPARE(t.flightState, sent.data.flightState);
            QVERIFY(qAbs(t.accelZ - sent.data.accelMg.z * 9.80665 / 1000.0) <= 0.001);
            QVERIFY(qAbs(t.gpsSpeed - sent.data.gps.speedKmh / 3.6) <= 0.01);
            QCOMPARE(t.gpsHasFix, sent.data.gps.hasFix);
            if (t.gpsHasFix)
                QVERIFY(qAbs(t.gpsLatitude - sent.data.gps.latitudeDeg) <= 1e-7);

            QCOMPARE(t.hasAltitude(), carriesAltitude);
            if (carriesAltitude) {
                QVERIFY(qAbs(t.altitude - sent.data.altitudeM) <= 0.01);
                QVERIFY(qAbs(t.pressure - sent.data.pressurePa / 1000.0) <= 0.0001);
            }
        }

        QCOMPARE(parser.rejectedCount(), quint64(0));

        // Every state shows up, in order
        qint32 state = StateIdle;
        for (const Telemetry &t : received) {
            QVERIFY(t.flightState == state || t.flightState == state + 1);
            state = t.flightState;
        }
        QCOMPARE(state, qint32(StateDescent));

        // The flight ends on the ground, which only altitude can show
        QCOMPARE(received.last().landed(), carriesAltitude);
        QVERIFY(!received[received.size() / 2].landed());
    }

    void loraSourceRecoversAfterChipReset()
    {
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        config.dio0Line = -1;
        LoRaSource source(config, std::move(bus));

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        source.start();
        QCOMPARE(link.size(), 1);

        fake->regs[0x01] = 0x09;
        QTRY_COMPARE(link.size(), 2);
        QVERIFY(!link[1][0].toBool());
        QTRY_COMPARE(link.size(), 3); // retry timer reconfigures the chip
        QVERIFY(link[2][0].toBool());
        QCOMPARE(fake->regs[0x01], quint8(0x8D));
    }

    void i2cSourceReadsQueuedPacketsInChunks()
    {
        auto bus = std::make_unique<FakeRadioBoard>();
        FakeRadioBoard *board = bus.get();
        board->queue = {kRocketLine, kRocketLine};
        I2cSource source(I2cConfig{}, std::move(bus));

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        QSignalSpy quality(&source, &TelemetrySource::signalQuality);
        source.start();
        QCOMPARE(link.size(), 1);
        QVERIFY(link[0][0].toBool());
        QVERIFY(link[0][1].toString().contains("0x08"));

        QTRY_COMPARE(bytes.size(), 2);
        QCOMPARE(bytes[0][0].toByteArray(), kRocketLine + '\n');
        QCOMPARE(bytes[1][0].toByteArray(), kRocketLine + '\n');
        QVERIFY(kRocketLine.size() > I2cSource::kMaxChunkBytes); // really split
        QVERIFY(board->largestRead <= I2cSource::kMaxChunkBytes);

        QCOMPARE(quality[0][0].toInt(), -72);
        QCOMPARE(quality[0][1].toDouble(), 8.5);
    }

    void i2cSourceRecoversWhenBoardStopsAnswering()
    {
        auto bus = std::make_unique<FakeRadioBoard>();
        FakeRadioBoard *board = bus.get();
        I2cSource source(I2cConfig{}, std::move(bus));

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        source.start();
        QCOMPARE(link.size(), 1);

        board->failReads = true;
        QTRY_COMPARE(link.size(), 2);
        QVERIFY(!link[1][0].toBool());

        board->failReads = false;
        QTRY_COMPARE(link.size(), 3); // retry timer reconnects
        QVERIFY(link[2][0].toBool());

        board->queue = {kRocketLine};
        QTRY_COMPARE(bytes.size(), 1);
    }

    void i2cSourceStripsAdafruitHeaderWhenConfigured()
    {
        auto bus = std::make_unique<FakeRadioBoard>();
        FakeRadioBoard *board = bus.get();
        board->queue = {kRadioHeadBroadcast + kRocketLine, "ab"};
        I2cConfig config;
        config.radioHeadHeader = true;
        I2cSource source(config, std::move(bus));

        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        QSignalSpy corrupted(&source, &TelemetrySource::packetCorrupted);
        source.start();
        QTRY_COMPARE(corrupted.size(), 1);
        QCOMPARE(bytes.size(), 1);
        QCOMPARE(bytes[0][0].toByteArray(), kRocketLine + '\n');
    }

    void i2cSourceNamesMissingBoard()
    {
        // This machine has no I2C bus; the message must say where it looked.
        I2cSource source(I2cConfig{});
        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        source.start();
        QCOMPARE(link.size(), 1);
        QVERIFY(!link[0][0].toBool());
        QVERIFY2(link[0][1].toString().contains("/dev/i2c-1"), qPrintable(link[0][1].toString()));
        source.stop();
    }

    void i2cDataReachesFrontendHub()
    {
        // The flight-day chain: I2C -> parser -> UDP -> hub (what the UI reads).
        auto bus = std::make_unique<FakeRadioBoard>();
        FakeRadioBoard *board = bus.get();
        I2cSource source(I2cConfig{}, std::move(bus));

        UdpTelemetryReceiver receiver;
        QVERIFY(receiver.start(0));
        UdpTelemetrySender sender(receiver.port());
        TelemetryHub hub;
        connect(&receiver, &UdpTelemetryReceiver::frameReceived, &hub, &TelemetryHub::addFrame);

        PacketParser parser;
        connect(&source, &TelemetrySource::bytesReceived, this, [&](const QByteArray &bytes) {
            for (const TelemetryFrame &frame : parser.feed(bytes))
                sender.send(frame);
        });

        source.start();
        board->queue = {kRocketLine};
        QTRY_COMPARE(hub.frameCount(), quint64(1));
        verifySample(hub.history().last().data);
    }

    void loraSourceSearchesBothChipSelectsByDefault()
    {
        // No reset pulse on a guessed pin until the wiring is confirmed.
        QCOMPARE(LoRaConfig{}.resetLine, -1);
        QCOMPARE(LoRaConfig{}.dio0Line, 22); // G0 on physical pin 15
        QVERIFY(LoRaConfig{}.spiDevice.isEmpty());

        // This machine has no SPI devices, so both probes fail and the
        // message names each one for whoever is debugging on the Pi.
        LoRaSource source(LoRaConfig{});
        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        source.start();
        QCOMPARE(link.size(), 1);
        QVERIFY(!link[0][0].toBool());
        const QString message = link[0][1].toString();
        QVERIFY2(message.contains("/dev/spidev0.0") && message.contains("/dev/spidev0.1"),
                 qPrintable(message));
        source.stop();
    }

    void loraSourceReadsPacketOnG0Edge()
    {
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        auto dio0 = std::make_unique<FakeDio0>();
        FakeDio0 *g0 = dio0.get();
        LoRaConfig config;
        config.resetLine = -1;
        LoRaSource source(config, std::move(bus), std::move(dio0));

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        source.start();
        QCOMPARE(link.size(), 1);
        QVERIFY(link[0][0].toBool());
        QVERIFY2(link[0][1].toString().startsWith("G0 interrupt, LoRa 433.0 MHz SF9 125 kHz"),
                 qPrintable(link[0][1].toString()));

        // Read well before the health check would get to it.
        fake->receive(kRadioHeadBroadcast + kRocketLine);
        g0->edge();
        QTRY_COMPARE_WITH_TIMEOUT(bytes.size(), 1, LoRaSource::kHealthCheckIntervalMs / 2);
        QCOMPARE(bytes[0][0].toByteArray(), kRocketLine + '\n');
        QCOMPARE(fake->regs[0x12], quint8(0)); // flags cleared, so G0 drops

        // A chip reset is still noticed with no edge at all.
        fake->regs[0x01] = 0x09;
        QTRY_COMPARE(link.size(), 2);
        QVERIFY(!link[1][0].toBool());
        QVERIFY(!g0->isOpen());
        QTRY_COMPARE(link.size(), 3);
        QVERIFY(link[2][1].toString().contains("G0 interrupt"));
        QCOMPARE(g0->opens, 2);
    }

    void loraSourcePollsWhenG0NeverRises()
    {
        // G0 claimed but not wired: packets only turn up on health checks.
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        LoRaSource source(config, std::move(bus), std::make_unique<FakeDio0>());

        QSignalSpy link(&source, &TelemetrySource::linkChanged);
        QSignalSpy bytes(&source, &TelemetrySource::bytesReceived);
        source.start();

        for (int packet = 1; packet <= 3; ++packet) {
            fake->receive(kRadioHeadBroadcast + kRocketLine);
            QTRY_COMPARE(bytes.size(), packet); // nothing lost meanwhile
        }
        QTRY_COMPARE(link.size(), 2);
        QVERIFY(link[1][0].toBool());
        QVERIFY2(link[1][1].toString().startsWith("Polling, no G0 signal: check GPIO 22"),
                 qPrintable(link[1][1].toString()));

        fake->receive(kRadioHeadBroadcast + kRocketLine);
        QTRY_COMPARE_WITH_TIMEOUT(bytes.size(), 4, LoRaSource::kHealthCheckIntervalMs / 2);
    }

    void radioPacketFromFirmwareReachesFrontendHub()
    {
        // Byte for byte what the rocket's radio task puts on air: its 4-byte
        // RadioHead-style header (to, from, id, flags), then the line from
        // Radio_BuildPacket(). Radio -> driver -> parser -> UDP -> hub.
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        config.dio0Line = -1;
        LoRaSource source(config, std::move(bus));

        UdpTelemetryReceiver receiver;
        QVERIFY(receiver.start(0));
        UdpTelemetrySender sender(receiver.port());
        TelemetryHub hub;
        connect(&receiver, &UdpTelemetryReceiver::frameReceived, &hub, &TelemetryHub::addFrame);

        PacketParser parser;
        connect(&source, &TelemetrySource::bytesReceived, this, [&](const QByteArray &bytes) {
            for (const TelemetryFrame &frame : parser.feed(bytes))
                sender.send(frame);
        });
        source.start();

        fake->receive(QByteArray("\xFF\x01\x07\x00", 4) + kRadioLineInFlight);
        QTRY_COMPARE(hub.frameCount(), quint64(1));
        verifyRadioSample(hub.history().last().data);

        fake->receive(QByteArray("\xFF\x01\x08\x00", 4) + kRadioLineOnPad);
        QTRY_COMPARE(hub.frameCount(), quint64(2));
        QCOMPARE(hub.history().last().data.timestampMs, 15000u);
        QCOMPARE(hub.history().last().data.gps.hasFix, false);
        QCOMPARE(parser.rejectedCount(), quint64(0));
    }

    void loraSourcePassesRocketLinesToBackendParser()
    {
        // Radio packet -> source -> parser, as wired in the worker.
        auto bus = std::make_unique<FakeRadio>();
        FakeRadio *fake = bus.get();
        LoRaConfig config;
        config.resetLine = -1;
        config.dio0Line = -1;
        LoRaSource source(config, std::move(bus));

        PacketParser parser;
        QList<TelemetryFrame> frames;
        connect(&source, &TelemetrySource::bytesReceived, this,
                [&](const QByteArray &bytes) { frames += parser.feed(bytes); });
        source.start();

        fake->receive(kRadioHeadBroadcast + kRocketLine);
        QTRY_COMPARE(frames.size(), 1);
        verifySample(frames[0].data);
    }
};

QTEST_GUILESS_MAIN(TestBackend)
#include "test_backend.moc"
