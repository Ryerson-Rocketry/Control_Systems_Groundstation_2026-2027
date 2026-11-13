#include "linuxhardware.h"

#include <QThread>

#ifdef Q_OS_LINUX
#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {
constexpr unsigned long kResetLowMs = 1;
constexpr unsigned long kResetSettleMs = 10;

#ifdef Q_OS_LINUX
QString systemError()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}
#else
const QString kUnsupported = QStringLiteral("Radio hardware access is only supported on Linux");
#endif
}

LinuxI2cBus::LinuxI2cBus(const QString &device, quint8 address)
    : m_device(device), m_address(address)
{
}

LinuxI2cBus::~LinuxI2cBus()
{
    close();
}

LinuxSpiBus::LinuxSpiBus(const QString &device, quint32 speedHz)
    : m_device(device), m_speedHz(speedHz)
{
}

LinuxSpiBus::~LinuxSpiBus()
{
    close();
}

LinuxGpioInterrupt::LinuxGpioInterrupt(const QString &chipDevice, int line)
    : m_chipDevice(chipDevice), m_line(line)
{
}

LinuxGpioInterrupt::~LinuxGpioInterrupt()
{
    close();
}

#ifdef Q_OS_LINUX

bool LinuxI2cBus::open(QString *error)
{
    if (m_fd >= 0)
        return true;

    m_fd = ::open(m_device.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        *error = QStringLiteral("Cannot open %1: %2 (is I2C enabled?)").arg(m_device, systemError());
        return false;
    }
    return true;
}

void LinuxI2cBus::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LinuxI2cBus::readRegister(quint8 reg, QByteArray *data)
{
    if (m_fd < 0 || data->isEmpty())
        return false;

    // Register write and data read in one transaction (repeated start),
    // so nothing else on the bus can slip in between.
    i2c_msg messages[2];
    messages[0].addr = m_address;
    messages[0].flags = 0;
    messages[0].len = 1;
    messages[0].buf = &reg;
    messages[1].addr = m_address;
    messages[1].flags = I2C_M_RD;
    messages[1].len = static_cast<quint16>(data->size());
    messages[1].buf = reinterpret_cast<quint8 *>(data->data());

    i2c_rdwr_ioctl_data transaction;
    transaction.msgs = messages;
    transaction.nmsgs = 2;
    return ioctl(m_fd, I2C_RDWR, &transaction) >= 0;
}

bool LinuxSpiBus::open(QString *error)
{
    if (m_fd >= 0)
        return true;

    m_fd = ::open(m_device.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0) {
        *error = QStringLiteral("Cannot open %1: %2 (is SPI enabled?)").arg(m_device, systemError());
        return false;
    }

    quint8 mode = SPI_MODE_0;
    quint8 bits = 8;
    if (ioctl(m_fd, SPI_IOC_WR_MODE, &mode) < 0
        || ioctl(m_fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0
        || ioctl(m_fd, SPI_IOC_WR_MAX_SPEED_HZ, &m_speedHz) < 0) {
        *error = QStringLiteral("Cannot configure %1: %2").arg(m_device, systemError());
        close();
        return false;
    }
    return true;
}

void LinuxSpiBus::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

bool LinuxSpiBus::transfer(QByteArray *data)
{
    if (m_fd < 0)
        return false;

    spi_ioc_transfer request;
    std::memset(&request, 0, sizeof(request));
    request.tx_buf = reinterpret_cast<quintptr>(data->data());
    request.rx_buf = reinterpret_cast<quintptr>(data->data());
    request.len = static_cast<quint32>(data->size());
    request.speed_hz = m_speedHz;
    request.bits_per_word = 8;
    return ioctl(m_fd, SPI_IOC_MESSAGE(1), &request) >= 0;
}

bool LinuxGpioInterrupt::open(QString *error)
{
    if (m_fd >= 0)
        return true;

    const int chip = ::open(m_chipDevice.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (chip < 0) {
        *error = QStringLiteral("Cannot open %1: %2").arg(m_chipDevice, systemError());
        return false;
    }

    gpio_v2_line_request request;
    std::memset(&request, 0, sizeof(request));
    request.offsets[0] = static_cast<quint32>(m_line);
    request.num_lines = 1;
    std::strncpy(request.consumer, "groundstation-radio-g0", sizeof(request.consumer) - 1);
    request.config.flags = GPIO_V2_LINE_FLAG_INPUT | GPIO_V2_LINE_FLAG_EDGE_RISING
                           | GPIO_V2_LINE_FLAG_BIAS_PULL_DOWN;

    const int requested = ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request);
    ::close(chip);
    if (requested < 0) {
        *error = QStringLiteral("Cannot claim GPIO %1 on %2: %3").arg(m_line).arg(m_chipDevice, systemError());
        return false;
    }

    // Non-blocking so clearEdges() can read until nothing is left.
    const int flags = fcntl(request.fd, F_GETFL);
    if (flags < 0 || fcntl(request.fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        *error = QStringLiteral("Cannot configure GPIO %1: %2").arg(m_line).arg(systemError());
        ::close(request.fd);
        return false;
    }
    m_fd = request.fd;
    return true;
}

void LinuxGpioInterrupt::close()
{
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void LinuxGpioInterrupt::clearEdges()
{
    if (m_fd < 0)
        return;
    gpio_v2_line_event events[16];
    while (::read(m_fd, events, sizeof(events)) > 0) {
    }
}

bool pulseGpioLow(const QString &chipDevice, int line, QString *error)
{
    const int chip = ::open(chipDevice.toLocal8Bit().constData(), O_RDWR | O_CLOEXEC);
    if (chip < 0) {
        *error = QStringLiteral("Cannot open %1: %2").arg(chipDevice, systemError());
        return false;
    }

    gpio_v2_line_request request;
    std::memset(&request, 0, sizeof(request));
    request.offsets[0] = static_cast<quint32>(line);
    request.num_lines = 1;
    std::strncpy(request.consumer, "groundstation-radio", sizeof(request.consumer) - 1);
    request.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
    request.config.num_attrs = 1;
    request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
    request.config.attrs[0].attr.values = 0; // start low: reset asserted
    request.config.attrs[0].mask = 1;

    const int requested = ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request);
    ::close(chip);
    if (requested < 0) {
        *error = QStringLiteral("Cannot claim GPIO %1 on %2: %3").arg(line).arg(chipDevice, systemError());
        return false;
    }

    QThread::msleep(kResetLowMs);
    gpio_v2_line_values values;
    values.bits = 1;
    values.mask = 1;
    const bool ok = ioctl(request.fd, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) >= 0;
    if (!ok)
        *error = QStringLiteral("Cannot release radio reset on GPIO %1: %2").arg(line).arg(systemError());
    ::close(request.fd);

    QThread::msleep(kResetSettleMs);
    return ok;
}

#else

bool LinuxI2cBus::open(QString *error)
{
    *error = kUnsupported;
    return false;
}

void LinuxI2cBus::close()
{
}

bool LinuxI2cBus::readRegister(quint8, QByteArray *)
{
    return false;
}

bool LinuxSpiBus::open(QString *error)
{
    *error = kUnsupported;
    return false;
}

void LinuxSpiBus::close()
{
}

bool LinuxSpiBus::transfer(QByteArray *)
{
    return false;
}

bool LinuxGpioInterrupt::open(QString *error)
{
    *error = kUnsupported;
    return false;
}

void LinuxGpioInterrupt::close()
{
}

void LinuxGpioInterrupt::clearEdges()
{
}

bool pulseGpioLow(const QString &, int, QString *error)
{
    *error = kUnsupported;
    return false;
}

#endif
