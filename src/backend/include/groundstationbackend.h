#ifndef GROUNDSTATIONBACKEND_H
#define GROUNDSTATIONBACKEND_H

#include <QObject>
#include <QString>
#include <QThread>

#include "i2csource.h"
#include "lorasource.h"
#include "telemetryframe.h"
#include "telemetryhub.h"
#include "udptransport.h"

class BackendWorker;

/*
 * groundstationbackend.h
 * Purpose:
 *   The only backend class the UI needs. Starts the worker thread,
 *   forwards requests (log export) to it, and reports results through
 *   signals. All methods and signals are on the UI thread.
 *
 * Telemetry reaches the hub over loopback UDP: worker -> UdpTelemetrySender
 * -> UdpTelemetryReceiver -> hub.
 */

enum class TelemetryInput
{
    LoRa,       // RFM9x wired straight to the Pi's SPI bus: flight day
    I2c,        // radio board's microcontroller on the Pi's I2C bus
    Serial,     // radio behind a USB serial port
    Simulated,  // demo stage (rule 15)
};

struct BackendConfig
{
    TelemetryInput input = TelemetryInput::LoRa;
    I2cConfig i2c;
    LoRaConfig lora;
    QString portName;           // Serial: empty = first serial port found
    qint32 baudRate = 115200;   // Serial: PLACEHOLDER
    int simIntervalMs = 500;
    QString logRoot;            // empty = app data folder
    quint16 udpPort = kDefaultUdpPort;  // 0 = any free port
};

class GroundstationBackend : public QObject
{
    Q_OBJECT

public:
    explicit GroundstationBackend(const BackendConfig &config, QObject *parent = nullptr);
    ~GroundstationBackend() override;

    void start();
    void stop();

    TelemetryHub *hub() { return &m_hub; }

public slots:
    void exportLogs(const QString &volumeRoot);

signals:
    void linkChanged(bool up, const QString &description);
    void signalQuality(int rssiDbm, double snrDb);
    void packetRejected(quint64 totalRejected);
    void logFailed(const QString &message);
    void udpFailed(const QString &message);
    void exportFinished(bool ok, const QString &message);

private:
    BackendConfig m_config;
    TelemetryHub m_hub;
    UdpTelemetryReceiver m_receiver;
    QThread m_thread;
    BackendWorker *m_worker = nullptr;
};

#endif // GROUNDSTATIONBACKEND_H
