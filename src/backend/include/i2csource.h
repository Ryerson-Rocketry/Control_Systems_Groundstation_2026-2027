#ifndef I2CSOURCE_H
#define I2CSOURCE_H

#include <memory>

#include <QTimer>

#include "telemetrysource.h"

/*
 * i2csource.h
 * Purpose:
 *   Alternative telemetry source (--i2c): the radio board's microcontroller,
 *   read over the Pi's I2C bus. The microcontroller receives the LoRa
 *   packets; the Pi polls it and hands each packet to the parser as one
 *   line. Retries on its own if the board is missing or stops answering.
 *
 * PROPOSED protocol, not yet confirmed with the board's firmware. The
 * board is an I2C slave at 0x08; the Pi is the master and polls every 20 ms:
 *   1. Read 4 bytes from register 0x00 (status):
 *        [0]    length of the oldest waiting packet, 0 = none
 *        [1..2] RSSI in dBm, int16 little-endian
 *        [3]    SNR in quarter dB, int8
 *   2. Read the packet from register 0x01 in chunks of at most 32 bytes
 *      (the usual microcontroller I2C buffer size); each read returns
 *      the next chunk, and the board drops the packet once it is all read.
 */

class I2cBus
{
public:
    virtual ~I2cBus() = default;
    virtual bool open(QString *error) = 0;
    virtual void close() = 0;
    // Writes reg, then reads data->size() bytes into data.
    virtual bool readRegister(quint8 reg, QByteArray *data) = 0;
};

struct I2cConfig
{
    QString device = QStringLiteral("/dev/i2c-1");
    quint8 address = 0x08;
    // Set if the board passes packets on with Adafruit's 4-byte header.
    bool radioHeadHeader = false;
};

class I2cSource : public TelemetrySource
{
    Q_OBJECT

public:
    // bus is for tests; by default the Pi's I2C device from config is used.
    explicit I2cSource(const I2cConfig &config, std::unique_ptr<I2cBus> bus = {},
                       QObject *parent = nullptr);

    void start() override;
    void stop() override;

    static constexpr quint8 kStatusRegister = 0x00;
    static constexpr quint8 kPacketRegister = 0x01;
    static constexpr int kStatusBytes = 4;
    static constexpr int kMaxChunkBytes = 32;

private:
    void tryStart();
    void poll();
    bool readPacket(int length, QByteArray *packet);
    void handlePacket(QByteArray payload);
    void fail(const QString &description);
    void reportLink(bool up, const QString &description);
    QString boardName() const;

    I2cConfig m_config;
    std::unique_ptr<I2cBus> m_bus;
    QTimer m_pollTimer;
    QTimer m_retryTimer;
    bool m_ready = false;
    QString m_lastDescription;
};

#endif // I2CSOURCE_H
