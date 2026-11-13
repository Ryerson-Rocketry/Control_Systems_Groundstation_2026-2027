#ifndef SX127X_H
#define SX127X_H

#include <QByteArray>
#include <QString>

/*
 * sx127x.h
 * Purpose:
 *   Receive-only driver for the SX127x LoRa chip inside the RFM9x
 *   module. Talks to the chip through SpiBus only, so tests can run it
 *   against a fake register map instead of real hardware.
 *
 * The chip stays in continuous receive with DIO0 (the board's G0 pin)
 * mapped to RxDone. poll() is called when G0 rises, and on a slow timer
 * as a health check; it reports received packets and faults (such as the
 * chip resetting after a brownout), after which begin() is retried.
 *
 * Every LoRaSettings value must match the rocket's radio exactly or
 * nothing will be received. Defaults are the rocket's settings:
 * 433.0 MHz, SF9, 125 kHz, CR 4/5, sync 0x12, explicit header,
 * 8-symbol preamble, CRC on.
 */

struct LoRaSettings
{
    double frequencyHz = 433.0e6;
    int spreadingFactor = 9;            // 7..12
    int bandwidthHz = 125000;
    int codingRateDenominator = 5;      // coding rate 4/5 .. 4/8
    quint8 syncWord = 0x12;
    quint16 preambleLength = 8;
};

struct LoRaPacket
{
    QByteArray payload;
    int rssiDbm = 0;
    double snrDb = 0.0;
};

class SpiBus
{
public:
    virtual ~SpiBus() = default;
    virtual bool open(QString *error) = 0;
    virtual void close() = 0;
    // Full duplex: sends data and overwrites it with what came back.
    virtual bool transfer(QByteArray *data) = 0;
};

// A GPIO input that reports rising edges through a file descriptor that
// becomes readable, so the event loop can wait on it.
class InterruptLine
{
public:
    virtual ~InterruptLine() = default;
    virtual bool open(QString *error) = 0;
    virtual void close() = 0;
    // Readable when an edge is pending; -1 while closed.
    virtual int fd() const = 0;
    // Consume pending edges so fd() stops being readable.
    virtual void clearEdges() = 0;
};

class Sx127x
{
public:
    enum class Event { None, PacketReceived, CrcError, Fault };

    explicit Sx127x(SpiBus *bus);

    // Talk to the chip on a different bus; call begin() afterwards.
    void setBus(SpiBus *bus) { m_bus = bus; }

    bool begin(const LoRaSettings &settings, QString *error);
    Event poll(LoRaPacket *packet);

    // The largest packet LoRa can carry.
    static constexpr int kMaxPayloadBytes = 255;

private:
    quint8 readRegister(quint8 reg);
    void writeRegister(quint8 reg, quint8 value);
    QByteArray readFifo(int length);
    void setMode(quint8 mode);

    SpiBus *m_bus;
    quint8 m_modeBase = 0;
    int m_rssiOffset = 0;
    bool m_busError = false;
};

#endif // SX127X_H
