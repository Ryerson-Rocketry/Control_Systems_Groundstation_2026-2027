#ifndef LINUXHARDWARE_H
#define LINUXHARDWARE_H

#include <QString>

#include "i2csource.h"
#include "sx127x.h"

/*
 * linuxhardware.h
 * Purpose:
 *   Raspberry Pi access for the radio: I2C through /dev/i2c-N, SPI
 *   through /dev/spidevX.Y, and the reset pin through the GPIO character
 *   device. On other systems these fail with a clear message, so the app
 *   still builds and runs in simulation.
 *
 * On the Pi, enable the bus in use (sudo raspi-config nonint do_i2c 0, or
 * do_spi 0) and add the user to the i2c (or spi and gpio) groups.
 */

class LinuxI2cBus : public I2cBus
{
public:
    LinuxI2cBus(const QString &device, quint8 address);
    ~LinuxI2cBus() override;

    bool open(QString *error) override;
    void close() override;
    bool readRegister(quint8 reg, QByteArray *data) override;

private:
    QString m_device;
    quint8 m_address;
    int m_fd = -1;
};

class LinuxSpiBus : public SpiBus
{
public:
    LinuxSpiBus(const QString &device, quint32 speedHz);
    ~LinuxSpiBus() override;

    bool open(QString *error) override;
    void close() override;
    bool transfer(QByteArray *data) override;

private:
    QString m_device;
    quint32 m_speedHz;
    int m_fd = -1;
};

// Drives a GPIO line low briefly, then high: the RFM9x reset pulse.
bool pulseGpioLow(const QString &chipDevice, int line, QString *error);

#endif // LINUXHARDWARE_H
