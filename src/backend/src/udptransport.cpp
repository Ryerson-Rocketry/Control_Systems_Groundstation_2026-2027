#include "udptransport.h"

#include <limits>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>

namespace {
const QString kReceivedKey = QStringLiteral("received_utc");
const QString kValuesKey = QStringLiteral("values");
}

QByteArray encodeDatagram(const TelemetryFrame &frame)
{
    const QList<TelemetryColumn> columns = telemetryColumns();
    const QList<double> values = telemetryValues(frame);

    // JSON has no NaN, so a column the packet lacked goes out as null.
    QJsonObject fields;
    for (qsizetype index = 0; index < values.size(); ++index) {
        fields.insert(QString::fromLatin1(columns[index].name),
                      hasValue(values[index]) ? QJsonValue(values[index]) : QJsonValue(QJsonValue::Null));
    }

    QJsonObject root;
    root.insert(kReceivedKey, frame.receivedAtUtc.toString(Qt::ISODateWithMs));
    root.insert(kValuesKey, fields);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool decodeDatagram(const QByteArray &datagram, TelemetryFrame *frame)
{
    const QJsonDocument document = QJsonDocument::fromJson(datagram);
    if (!document.isObject())
        return false;
    const QJsonObject root = document.object();

    const QDateTime received = QDateTime::fromString(root.value(kReceivedKey).toString(), Qt::ISODateWithMs);
    if (!received.isValid())
        return false;

    const QJsonObject fields = root.value(kValuesKey).toObject();
    const QList<TelemetryColumn> columns = telemetryColumns();
    QList<double> values;
    values.reserve(columns.size());
    for (const TelemetryColumn &column : columns) {
        // Every column must be named: null means not sent, absent is an error.
        const QJsonValue value = fields.value(QString::fromLatin1(column.name));
        if (value.isNull())
            values.append(std::numeric_limits<double>::quiet_NaN());
        else if (value.isDouble())
            values.append(value.toDouble());
        else
            return false;
    }

    if (!setTelemetryValues(values, frame))
        return false;
    frame->receivedAtUtc = received;
    return true;
}

UdpTelemetrySender::UdpTelemetrySender(quint16 port, QObject *parent)
    : QObject(parent), m_socket(this), m_port(port)
{
}

void UdpTelemetrySender::send(const TelemetryFrame &frame)
{
    const QByteArray datagram = encodeDatagram(frame);
    const bool ok = m_socket.writeDatagram(datagram, QHostAddress::LocalHost, m_port) == datagram.size();
    if (!ok && !m_failing)
        emit sendFailed(QStringLiteral("Telemetry UDP send failed: %1").arg(m_socket.errorString()));
    m_failing = !ok;
}

UdpTelemetryReceiver::UdpTelemetryReceiver(QObject *parent)
    : QObject(parent), m_socket(this)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpTelemetryReceiver::readPending);
}

bool UdpTelemetryReceiver::start(quint16 port)
{
    return m_socket.bind(QHostAddress::LocalHost, port);
}

void UdpTelemetryReceiver::stop()
{
    m_socket.close();
}

void UdpTelemetryReceiver::readPending()
{
    while (m_socket.hasPendingDatagrams()) {
        TelemetryFrame frame;
        if (decodeDatagram(m_socket.receiveDatagram().data(), &frame))
            emit frameReceived(frame);
        else
            ++m_rejected;
    }
}
