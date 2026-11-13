#include "backendworker.h"

#include "i2csource.h"
#include "lorasource.h"
#include "serialsource.h"
#include "simsource.h"
#include "telemetrylogger.h"
#include "udptransport.h"

BackendWorker::BackendWorker(const BackendConfig &config, QObject *parent)
    : QObject(parent), m_config(config)
{
}

void BackendWorker::start()
{
    // Created here, not in the constructor, so their timers and sockets
    // belong to the worker thread.
    m_logger = new TelemetryLogger(m_config.logRoot, this);
    connect(m_logger, &TelemetryLogger::writeFailed, this, &BackendWorker::logFailed);
    m_logger->startSession();

    m_udpSender = new UdpTelemetrySender(m_config.udpPort, this);
    connect(m_udpSender, &UdpTelemetrySender::sendFailed, this, &BackendWorker::udpFailed);

    switch (m_config.input) {
    case TelemetryInput::I2c:
        m_source = new I2cSource(m_config.i2c, {}, this);
        break;
    case TelemetryInput::LoRa:
        m_source = new LoRaSource(m_config.lora, {}, {}, this);
        break;
    case TelemetryInput::Serial:
        m_source = new SerialSource(m_config.portName, m_config.baudRate, this);
        break;
    case TelemetryInput::Simulated:
        m_source = new SimSource(m_config.simIntervalMs, this);
        break;
    }

    connect(m_source, &TelemetrySource::bytesReceived, this, &BackendWorker::handleBytes);
    connect(m_source, &TelemetrySource::linkChanged, this, &BackendWorker::linkChanged);
    connect(m_source, &TelemetrySource::signalQuality, this, &BackendWorker::signalQuality);
    connect(m_source, &TelemetrySource::packetCorrupted, this, &BackendWorker::handleCorruptPacket);
    m_source->start();
}

void BackendWorker::stop()
{
    if (m_source)
        m_source->stop();
}

void BackendWorker::exportLogs(const QString &volumeRoot)
{
    QString error;
    const bool ok = m_logger->exportTo(volumeRoot, &error);
    emit exportFinished(ok, ok ? QStringLiteral("Logs saved to %1").arg(volumeRoot) : error);
}

void BackendWorker::handleBytes(const QByteArray &bytes)
{
    const quint64 rejectedBefore = m_parser.rejectedCount();
    const QList<TelemetryFrame> frames = m_parser.feed(bytes);

    // Log first: UDP can drop a datagram, the log must not miss a frame.
    for (const TelemetryFrame &frame : frames) {
        m_logger->log(frame);
        m_udpSender->send(frame);
    }

    if (m_parser.rejectedCount() != rejectedBefore)
        emit packetRejected(m_parser.rejectedCount() + m_corruptPackets);
}

void BackendWorker::handleCorruptPacket()
{
    ++m_corruptPackets;
    emit packetRejected(m_parser.rejectedCount() + m_corruptPackets);
}
