#ifndef TELEMETRYSOURCE_H
#define TELEMETRYSOURCE_H

#include <QByteArray>
#include <QObject>
#include <QString>

/*
 * telemetrysource.h
 * Purpose:
 *   Where telemetry bytes come from. The rest of the backend only sees
 *   this interface, so the same code path runs for the radio on flight
 *   day and the simulator in the demo. Receive only: the groundstation
 *   sends nothing to the rocket.
 */

class TelemetrySource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void bytesReceived(const QByteArray &bytes);
    void linkChanged(bool up, const QString &description);
    // Radio sources only: strength of the packet just received.
    void signalQuality(int rssiDbm, double snrDb);
    // A packet arrived but failed the radio's own CRC check.
    void packetCorrupted();
};

#endif // TELEMETRYSOURCE_H
