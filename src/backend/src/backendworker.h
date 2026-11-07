#ifndef BACKENDWORKER_H
#define BACKENDWORKER_H

#include <QObject>

#include "groundstationbackend.h"
#include "packetparser.h"

class TelemetryLogger;
class TelemetrySource;
class UdpTelemetrySender;

/*
 * backendworker.h
 * Purpose:
 *   Everything that must keep running if the UI is busy: radio I/O,
 *   parsing, logging, and publishing frames over UDP. Lives on the
 *   backend's worker thread.
 */

class BackendWorker : public QObject
{
    Q_OBJECT

public:
    explicit BackendWorker(const BackendConfig &config, QObject *parent = nullptr);

    void start();
    void stop();
    void exportLogs(const QString &volumeRoot);

signals:
    void linkChanged(bool up, const QString &description);
    void signalQuality(int rssiDbm, double snrDb);
    void packetRejected(quint64 totalRejected);
    void logFailed(const QString &message);
    void udpFailed(const QString &message);
    void exportFinished(bool ok, const QString &message);

private:
    void handleBytes(const QByteArray &bytes);
    void handleCorruptPacket();

    BackendConfig m_config;
    TelemetrySource *m_source = nullptr;
    TelemetryLogger *m_logger = nullptr;
    UdpTelemetrySender *m_udpSender = nullptr;
    PacketParser m_parser;
    quint64 m_corruptPackets = 0;
};

#endif // BACKENDWORKER_H
