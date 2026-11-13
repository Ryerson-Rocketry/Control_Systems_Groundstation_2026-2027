#include "lorasource.h"

#include "linuxhardware.h"

namespace {
// Packets the health check finds without a G0 edge before giving up on G0.
// More than one, so a packet landing between an edge and its handling
// doesn't count as the pin being broken.
constexpr int kMaxMissedEdges = 3;
constexpr int kRetryIntervalMs = 2000;
constexpr int kRadioHeadHeaderBytes = 4;
const QStringList kAutoDetectDevices = {QStringLiteral("/dev/spidev0.0"), QStringLiteral("/dev/spidev0.1")};
}

LoRaSource::LoRaSource(const LoRaConfig &config, std::unique_ptr<SpiBus> bus,
                       std::unique_ptr<InterruptLine> dio0, QObject *parent)
    : TelemetrySource(parent), m_config(config), m_bus(std::move(bus)),
      m_busInjected(m_bus != nullptr), m_radio(m_bus.get()), m_dio0(std::move(dio0)),
      m_pollTimer(this), m_retryTimer(this)
{
    if (!m_dio0 && m_config.dio0Line >= 0)
        m_dio0 = std::make_unique<LinuxGpioInterrupt>(m_config.gpioChip, m_config.dio0Line);

    connect(&m_pollTimer, &QTimer::timeout, this, [this] { poll(false); });

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
    stopInterrupt();
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

    const LoRaSettings &radio = m_config.radio;
    // The header shows only the start of the link line, so how packets are
    // being received goes in front of this (see startInterrupt).
    m_listenDescription = QStringLiteral("LoRa %1 MHz SF%2 %3 kHz")
                              .arg(radio.frequencyHz / 1.0e6, 0, 'f', 1)
                              .arg(radio.spreadingFactor)
                              .arg(radio.bandwidthHz / 1000.0);
    if (!m_device.isEmpty())
        m_listenDescription += QStringLiteral(" on %1").arg(m_device);

    startInterrupt();
}

void LoRaSource::startInterrupt()
{
    if (!m_dio0) {
        m_pollTimer.start(kPollIntervalMs);
        reportLink(true, QStringLiteral("Polling, %1").arg(m_listenDescription));
        return;
    }

    QString error;
    if (!m_dio0->open(&error)) {
        fallBackToPolling(QStringLiteral("G0 unavailable: %1").arg(error));
        return;
    }

    // Edges from before the radio was configured mean nothing.
    m_dio0->clearEdges();
    m_missedEdges = 0;
    m_dio0Notifier = new QSocketNotifier(m_dio0->fd(), QSocketNotifier::Read, this);
    connect(m_dio0Notifier, &QSocketNotifier::activated, this, &LoRaSource::onDio0);
    m_pollTimer.start(kHealthCheckIntervalMs);
    reportLink(true, QStringLiteral("G0 interrupt, %1").arg(m_listenDescription));
}

void LoRaSource::stopInterrupt()
{
    // Can be reached from the notifier's own slot, so no direct delete.
    if (m_dio0Notifier) {
        m_dio0Notifier->setEnabled(false);
        m_dio0Notifier->deleteLater();
        m_dio0Notifier = nullptr;
    }
    if (m_dio0)
        m_dio0->close();
}

void LoRaSource::fallBackToPolling(const QString &reason)
{
    stopInterrupt();
    m_pollTimer.start(kPollIntervalMs);
    reportLink(true, QStringLiteral("Polling, %1, %2").arg(reason, m_listenDescription));
}

void LoRaSource::onDio0()
{
    m_dio0->clearEdges();
    poll(true);
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

void LoRaSource::poll(bool fromEdge)
{
    LoRaPacket packet;
    const Sx127x::Event event = m_radio.poll(&packet);

    // While G0 drives receive, the health check should find nothing waiting.
    if (m_dio0Notifier
        && (event == Sx127x::Event::PacketReceived || event == Sx127x::Event::CrcError)) {
        m_missedEdges = fromEdge ? 0 : m_missedEdges + 1;
    }

    switch (event) {
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
        stopInterrupt();
        m_retryTimer.start();
        reportLink(false, QStringLiteral("Radio fault, restarting radio"));
        break;
    case Sx127x::Event::None:
        break;
    }

    if (m_dio0Notifier && m_missedEdges >= kMaxMissedEdges)
        fallBackToPolling(QStringLiteral("no G0 signal: check GPIO %1").arg(m_config.dio0Line));
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
