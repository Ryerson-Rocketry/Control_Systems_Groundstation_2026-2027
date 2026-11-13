#ifndef LORASOURCE_H
#define LORASOURCE_H

#include <memory>

#include <QSocketNotifier>
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
 *
 * Packets are read the moment the radio's G0 (DIO0) pin rises. A slow
 * timer also checks the chip so faults are still noticed. If G0 can't be
 * claimed, or packets keep turning up without a G0 edge (pin not wired),
 * it falls back to fast polling and says so in the link description.
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
    // GPIO of the radio's G0 (DIO0) pin, or -1 to poll instead.
    // 22 = physical pin 15 on the Pi 4B header.
    int dio0Line = 22;
    // The rocket uses Adafruit's RFM9x library, which puts a 4-byte
    // to/from/id/flags header (RadioHead format) on every packet.
    bool radioHeadHeader = true;
    LoRaSettings radio;
};

class LoRaSource : public TelemetrySource
{
    Q_OBJECT

public:
    // bus and dio0 are for tests; by default the Pi's SPI devices and
    // config.dio0Line are used.
    explicit LoRaSource(const LoRaConfig &config, std::unique_ptr<SpiBus> bus = {},
                        std::unique_ptr<InterruptLine> dio0 = {}, QObject *parent = nullptr);

    // How often the chip is checked while G0 drives receive.
    static constexpr int kHealthCheckIntervalMs = 500;
    // Fallback poll rate without G0; far shorter than any LoRa packet's airtime.
    static constexpr int kPollIntervalMs = 10;

    void start() override;
    void stop() override;

private:
    void tryStart();
    bool startRadio(QString *error);
    void startInterrupt();
    void stopInterrupt();
    void fallBackToPolling(const QString &reason);
    void onDio0();
    void poll(bool fromEdge);
    void handlePacket(const LoRaPacket &packet);
    void reportLink(bool up, const QString &description);

    LoRaConfig m_config;
    std::unique_ptr<SpiBus> m_bus;
    bool m_busInjected;
    Sx127x m_radio;
    QString m_device;
    std::unique_ptr<InterruptLine> m_dio0;
    QSocketNotifier *m_dio0Notifier = nullptr;
    int m_missedEdges = 0;
    QString m_listenDescription;
    QTimer m_pollTimer;
    QTimer m_retryTimer;
    bool m_ready = false;
    QString m_lastDescription;
};

#endif // LORASOURCE_H
