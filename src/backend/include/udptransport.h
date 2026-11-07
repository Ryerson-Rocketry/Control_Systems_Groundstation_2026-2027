#ifndef UDPTRANSPORT_H
#define UDPTRANSPORT_H

#include <QObject>
#include <QUdpSocket>

#include "telemetryframe.h"

/*
 * udptransport.h
 * Purpose:
 *   Carries telemetry from the backend to the frontend as one JSON
 *   datagram per frame:
 *     {"received_utc":"...","values":{"timestamp":1234,"altitude":152.3,...}}
 * Values are in real units (already divided by their scale factor).
 *
 * Both ends use the loopback address only, and the receiver sends
 * nothing back. Rule 14 bans network control in the shipped build, so
 * this must never bind to a real interface or accept commands.
 */

inline constexpr quint16 kDefaultUdpPort = 45454;

QByteArray encodeDatagram(const TelemetryFrame &frame);
bool decodeDatagram(const QByteArray &datagram, TelemetryFrame *frame);

class UdpTelemetrySender : public QObject
{
    Q_OBJECT

public:
    explicit UdpTelemetrySender(quint16 port, QObject *parent = nullptr);

    void send(const TelemetryFrame &frame);

signals:
    void sendFailed(const QString &message);

private:
    QUdpSocket m_socket;
    quint16 m_port;
    bool m_failing = false;
};

class UdpTelemetryReceiver : public QObject
{
    Q_OBJECT

public:
    explicit UdpTelemetryReceiver(QObject *parent = nullptr);

    // Port 0 picks any free port; read it back with port().
    bool start(quint16 port);
    void stop();

    quint16 port() const { return m_socket.localPort(); }
    QHostAddress address() const { return m_socket.localAddress(); }
    QString errorString() const { return m_socket.errorString(); }
    quint64 rejectedCount() const { return m_rejected; }

signals:
    void frameReceived(const TelemetryFrame &frame);

private:
    void readPending();

    QUdpSocket m_socket;
    quint64 m_rejected = 0;
};

#endif // UDPTRANSPORT_H
