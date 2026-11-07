#include "serialsource.h"

#include <QSerialPortInfo>

namespace {
constexpr int kRetryIntervalMs = 2000;
}

SerialSource::SerialSource(const QString &portName, qint32 baudRate, QObject *parent)
    : TelemetrySource(parent), m_port(this), m_retryTimer(this),
      m_portName(portName), m_baudRate(baudRate)
{
    m_retryTimer.setInterval(kRetryIntervalMs);
    connect(&m_retryTimer, &QTimer::timeout, this, &SerialSource::tryOpen);

    connect(&m_port, &QSerialPort::readyRead, this, [this] {
        emit bytesReceived(m_port.readAll());
    });
    connect(&m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::ResourceError) {
            const QString reason = m_port.errorString();
            m_port.close();
            reportLink(false, QStringLiteral("Radio disconnected: %1").arg(reason));
        }
    });
}

void SerialSource::start()
{
    m_retryTimer.start();
    tryOpen();
}

void SerialSource::stop()
{
    m_retryTimer.stop();
    m_port.close();
    reportLink(false, QStringLiteral("Radio stopped"));
}

void SerialSource::tryOpen()
{
    if (m_port.isOpen())
        return;

    QString name = m_portName;
    if (name.isEmpty()) {
        const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
        if (ports.isEmpty()) {
            reportLink(false, QStringLiteral("No radio found"));
            return;
        }
        name = ports.first().portName();
    }

    m_port.setPortName(name);
    m_port.setBaudRate(m_baudRate);
    if (m_port.open(QIODevice::ReadWrite))
        reportLink(true, QStringLiteral("Radio connected on %1").arg(name));
    else
        reportLink(false, QStringLiteral("Cannot open %1: %2").arg(name, m_port.errorString()));
}

void SerialSource::reportLink(bool up, const QString &description)
{
    // Retries fail every 2 s; only tell the UI when something changed.
    if (description == m_lastDescription)
        return;
    m_lastDescription = description;
    emit linkChanged(up, description);
}
