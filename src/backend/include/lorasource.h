#ifndef LORASOURCE_H
#define LORASOURCE_H

#include <memory>

#include <QTimer>

#include "sx127x.h"
#include "telemetrysource.h"

/*
 * lorasource.h
 * Purpose:
 *   The flight-day telemetry source (the default): an RFM9x on the
 *   Raspberry Pi's SPI bus. Each LoRa packet is handed to the parser as one line. Like the
 *   serial source, it keeps retrying on its own if the radio is missing
 *   or faults.
 */

struct LoRaConfig
{
    // Empty = try /dev/spidev0.0 then /dev/spidev0.1 and use whichever
    // one the radio answers on, so the chip-select wiring needn't be known.
    QString spiDevice;
    quint32 spiSpeedHz = 5000000;
    QString gpioChip = QStringLiteral("/dev/gpiochip0");
    // GPIO of the radio's RESET pin, or -1 for none. Off until the wiring
    // is confirmed: pulsing a guessed pin could drive something else. The
    // radio starts fine without it; it only helps recover a hung chip.
    int resetLine = -1;
    // The rocket uses Adafruit's RFM9x library, which puts a 4-byte
    // to/from/id/flags header (RadioHead format) on every packet.
    bool radioHeadHeader = true;
    LoRaSettings radio;
};

class LoRaSource : public TelemetrySource
{
    Q_OBJECT

public:
    // bus is for tests; by default the Pi's SPI devices are used.
    explicit LoRaSource(const LoRaConfig &config, std::unique_ptr<SpiBus> bus = {},
                        QObject *parent = nullptr);

    void start() override;
    void stop() override;

private:
    void tryStart();
    bool startRadio(QString *error);
    void poll();
    void handlePacket(const LoRaPacket &packet);
    void reportLink(bool up, const QString &description);

    LoRaConfig m_config;
    std::unique_ptr<SpiBus> m_bus;
    bool m_busInjected;
    Sx127x m_radio;
    QString m_device;
    QTimer m_pollTimer;
    QTimer m_retryTimer;
    bool m_ready = false;
    QString m_lastDescription;
};

#endif // LORASOURCE_H
