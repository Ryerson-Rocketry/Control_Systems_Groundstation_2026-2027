#include "i2csource.h"

#include "linuxhardware.h"

namespace {
constexpr int kPollIntervalMs = 20;
constexpr int kRetryIntervalMs = 2000;
constexpr int kRadioHeadHeaderBytes = 4;
// Drain a backlog quickly, but never block the worker thread for long.
constexpr int kMaxPacketsPerPoll = 8;
}

I2cSource::I2cSource(const I2cConfig &config, std::unique_ptr<I2cBus> bus, QObject *parent)
    : TelemetrySource(parent), m_config(config),
      m_bus(bus ? std::move(bus) : std::make_unique<LinuxI2cBus>(config.device, config.address)),
      m_pollTimer(this), m_retryTimer(this)
{
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &I2cSource::poll);

    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &I2cSource::tryStart);
}

void I2cSource::start()
{
    m_retryTimer.start();
    tryStart();
}

void I2cSource::stop()
{
    m_retryTimer.stop();
    m_pollTimer.stop();
    m_ready = false;
    m_bus->close();
    reportLink(false, QStringLiteral("Radio board stopped"));
}

void I2cSource::tryStart()
{
    if (m_ready)
        return;

    QString error;
    if (!m_bus->open(&error)) {
        reportLink(false, error);
        return;
    }

    // The board must answer before the link counts as up.
    QByteArray status(kStatusBytes, '\0');
    if (!m_bus->readRegister(kStatusRegister, &status)) {
        m_bus->close();
        reportLink(false, QStringLiteral("No answer from %1").arg(boardName()));
        return;
    }

    m_ready = true;
    m_retryTimer.stop();
    m_pollTimer.start();
    reportLink(true, QStringLiteral("Radio board connected: %1").arg(boardName()));
}

void I2cSource::poll()
{
    for (int count = 0; count < kMaxPacketsPerPoll; ++count) {
        QByteArray status(kStatusBytes, '\0');
        if (!m_bus->readRegister(kStatusRegister, &status)) {
            fail(QStringLiteral("Lost contact with %1, retrying").arg(boardName()));
            return;
        }

        const int length = quint8(status[0]);
        if (length == 0)
            return;

        QByteArray packet;
        if (!readPacket(length, &packet)) {
            fail(QStringLiteral("Packet read from %1 failed, retrying").arg(boardName()));
            return;
        }

        const auto rssiDbm = static_cast<qint16>(quint8(status[1]) | (quint8(status[2]) << 8));
        const double snrDb = static_cast<qint8>(status[3]) / 4.0;
        emit signalQuality(rssiDbm, snrDb);
        handlePacket(packet);
    }
}

bool I2cSource::readPacket(int length, QByteArray *packet)
{
    while (packet->size() < length) {
        QByteArray chunk(qMin<qsizetype>(kMaxChunkBytes, length - packet->size()), '\0');
        if (!m_bus->readRegister(kPacketRegister, &chunk))
            return false;
        packet->append(chunk);
    }
    return true;
}

void I2cSource::handlePacket(QByteArray payload)
{
    if (m_config.radioHeadHeader) {
        if (payload.size() <= kRadioHeadHeaderBytes) {
            emit packetCorrupted();
            return;
        }
        payload.remove(0, kRadioHeadHeaderBytes);
    }

    // One radio packet is one line for the parser.
    if (!payload.endsWith('\n'))
        payload.append('\n');
    emit bytesReceived(payload);
}

void I2cSource::fail(const QString &description)
{
    m_ready = false;
    m_pollTimer.stop();
    m_bus->close();
    m_retryTimer.start();
    reportLink(false, description);
}

void I2cSource::reportLink(bool up, const QString &description)
{
    // Retries fail every 2 s; only tell the UI when something changed.
    if (description == m_lastDescription)
        return;
    m_lastDescription = description;
    emit linkChanged(up, description);
}

QString I2cSource::boardName() const
{
    return QStringLiteral("radio board at 0x%1 on %2")
        .arg(uint(m_config.address), 2, 16, QLatin1Char('0'))
        .arg(m_config.device);
}
