#include "lorasource.h"

#include "linuxhardware.h"

namespace {
constexpr int kPollIntervalMs = 10;
constexpr int kRetryIntervalMs = 2000;
constexpr int kRadioHeadHeaderBytes = 4;
const QStringList kAutoDetectDevices = {QStringLiteral("/dev/spidev0.0"), QStringLiteral("/dev/spidev0.1")};
}

LoRaSource::LoRaSource(const LoRaConfig &config, std::unique_ptr<SpiBus> bus, QObject *parent)
    : TelemetrySource(parent), m_config(config), m_bus(std::move(bus)),
      m_busInjected(m_bus != nullptr), m_radio(m_bus.get()), m_pollTimer(this), m_retryTimer(this)
{
    // Polling, not the DIO0 interrupt pin: one less GPIO to get wrong, and
    // 10 ms is far shorter than the airtime of the smallest LoRa packet.
    m_pollTimer.setInterval(kPollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, &LoRaSource::poll);

    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &LoRaSource::tryStart);
}

void LoRaSource::start()
{
    m_retryTimer.start();
    tryStart();
}

void LoRaSource::stop()
{
    m_retryTimer.stop();
    m_pollTimer.stop();
    m_ready = false;
    if (m_bus)
        m_bus->close();
    reportLink(false, QStringLiteral("Radio stopped"));
}

void LoRaSource::tryStart()
{
    if (m_ready)
        return;

    QString error;
    if (m_config.resetLine >= 0 && !pulseGpioLow(m_config.gpioChip, m_config.resetLine, &error)) {
        reportLink(false, error);
        return;
    }

    if (m_busInjected) {
        if (!startRadio(&error)) {
            reportLink(false, error);
            return;
        }
    } else {
        // The radio only answers on its own chip select; on the other one
        // the version register reads back 0x00 and begin() stops before
        // writing anything, so probing is harmless.
        const QStringList devices = m_config.spiDevice.isEmpty() ? kAutoDetectDevices
                                                                 : QStringList{m_config.spiDevice};
        QStringList failures;
        for (const QString &device : devices) {
            m_bus = std::make_unique<LinuxSpiBus>(device, m_config.spiSpeedHz);
            m_radio.setBus(m_bus.get());
            if (startRadio(&error)) {
                m_device = device;
                break;
            }
            failures.append(error.contains(device) ? error : QStringLiteral("%1: %2").arg(device, error));
        }
        if (m_device.isEmpty()) {
            reportLink(false, QStringLiteral("Radio not found. %1").arg(failures.join(QStringLiteral("; "))));
            return;
        }
    }

    m_ready = true;
    m_retryTimer.stop();
    m_pollTimer.start();

    const LoRaSettings &radio = m_config.radio;
    QString description = QStringLiteral("LoRa radio listening on %1 MHz (SF%2, %3 kHz)")
                              .arg(radio.frequencyHz / 1.0e6, 0, 'f', 1)
                              .arg(radio.spreadingFactor)
                              .arg(radio.bandwidthHz / 1000.0);
    if (!m_device.isEmpty())
        description += QStringLiteral(" on %1").arg(m_device);
    reportLink(true, description);
}

bool LoRaSource::startRadio(QString *error)
{
    if (!m_bus->open(error))
        return false;
    if (!m_radio.begin(m_config.radio, error)) {
        m_bus->close();
        return false;
    }
    return true;
}

void LoRaSource::poll()
{
    LoRaPacket packet;
    switch (m_radio.poll(&packet)) {
    case Sx127x::Event::PacketReceived:
        handlePacket(packet);
        break;
    case Sx127x::Event::CrcError:
        emit packetCorrupted();
        break;
    case Sx127x::Event::Fault:
        m_ready = false;
        m_device.clear(); // search both chip selects again
        m_pollTimer.stop();
        m_retryTimer.start();
        reportLink(false, QStringLiteral("Radio fault, restarting radio"));
        break;
    case Sx127x::Event::None:
        break;
    }
}

void LoRaSource::handlePacket(const LoRaPacket &packet)
{
    QByteArray payload = packet.payload;
    if (m_config.radioHeadHeader) {
        if (payload.size() <= kRadioHeadHeaderBytes) {
            emit packetCorrupted();
            return;
        }
        payload.remove(0, kRadioHeadHeaderBytes);
    }

    // One LoRa packet is one line for the parser.
    if (!payload.endsWith('\n'))
        payload.append('\n');

    emit signalQuality(packet.rssiDbm, packet.snrDb);
    emit bytesReceived(payload);
}

void LoRaSource::reportLink(bool up, const QString &description)
{
    // Retries fail every 2 s; only tell the UI when something changed.
    if (description == m_lastDescription)
        return;
    m_lastDescription = description;
    emit linkChanged(up, description);
}
