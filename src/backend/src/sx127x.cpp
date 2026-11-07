#include "sx127x.h"

#include <cmath>
#include <iterator>

/*
 * Register map and bit values: Semtech SX1276/77/78/79 datasheet,
 * LoRa mode register table.
 */

namespace {
constexpr quint8 RegFifo = 0x00;
constexpr quint8 RegOpMode = 0x01;
constexpr quint8 RegFrfMsb = 0x06;
constexpr quint8 RegFrfMid = 0x07;
constexpr quint8 RegFrfLsb = 0x08;
constexpr quint8 RegFifoAddrPtr = 0x0D;
constexpr quint8 RegFifoRxBaseAddr = 0x0F;
constexpr quint8 RegFifoRxCurrentAddr = 0x10;
constexpr quint8 RegIrqFlags = 0x12;
constexpr quint8 RegRxNbBytes = 0x13;
constexpr quint8 RegPktSnrValue = 0x19;
constexpr quint8 RegPktRssiValue = 0x1A;
constexpr quint8 RegModemConfig1 = 0x1D;
constexpr quint8 RegModemConfig2 = 0x1E;
constexpr quint8 RegPreambleMsb = 0x20;
constexpr quint8 RegPreambleLsb = 0x21;
constexpr quint8 RegModemConfig3 = 0x26;
constexpr quint8 RegSyncWord = 0x39;
constexpr quint8 RegDioMapping1 = 0x40;
constexpr quint8 RegVersion = 0x42;

constexpr quint8 kWriteBit = 0x80;

constexpr quint8 ModeLongRange = 0x80;
constexpr quint8 ModeLowFrequency = 0x08;
constexpr quint8 ModeSleep = 0x00;
constexpr quint8 ModeStandby = 0x01;
constexpr quint8 ModeRxContinuous = 0x05;
constexpr quint8 kModeCheckMask = 0x8F;

constexpr quint8 IrqRxDone = 0x40;
constexpr quint8 IrqPayloadCrcError = 0x20;
constexpr quint8 kClearAllIrqs = 0xFF;

constexpr quint8 RxPayloadCrcOn = 0x04;
constexpr quint8 AgcAutoOn = 0x04;
constexpr quint8 LowDataRateOptimize = 0x08;

constexpr quint8 kChipVersion = 0x12;
constexpr double kOscillatorHz = 32.0e6;
constexpr double kLowBandMaxHz = 525.0e6;
constexpr double kMinFrequencyHz = 137.0e6;
constexpr double kMaxFrequencyHz = 1020.0e6;

// Index in this table is the chip's bandwidth code.
constexpr int kBandwidthsHz[] = {7800, 10400, 15600, 20800, 31250, 41700, 62500, 125000, 250000, 500000};

int bandwidthCode(int bandwidthHz)
{
    for (int code = 0; code < int(std::size(kBandwidthsHz)); ++code) {
        if (kBandwidthsHz[code] == bandwidthHz)
            return code;
    }
    return -1;
}

QString validate(const LoRaSettings &settings)
{
    if (settings.frequencyHz < kMinFrequencyHz || settings.frequencyHz > kMaxFrequencyHz)
        return QStringLiteral("LoRa frequency %1 Hz is out of range").arg(settings.frequencyHz);
    if (settings.spreadingFactor < 7 || settings.spreadingFactor > 12)
        return QStringLiteral("LoRa spreading factor must be 7 to 12");
    if (bandwidthCode(settings.bandwidthHz) < 0)
        return QStringLiteral("Unsupported LoRa bandwidth %1 Hz").arg(settings.bandwidthHz);
    if (settings.codingRateDenominator < 5 || settings.codingRateDenominator > 8)
        return QStringLiteral("LoRa coding rate must be 4/5 to 4/8");
    return {};
}
}

Sx127x::Sx127x(SpiBus *bus) : m_bus(bus)
{
}

bool Sx127x::begin(const LoRaSettings &settings, QString *error)
{
    m_busError = false;

    if (const QString problem = validate(settings); !problem.isEmpty()) {
        *error = problem;
        return false;
    }

    const quint8 version = readRegister(RegVersion);
    if (m_busError) {
        *error = QStringLiteral("SPI transfer to the radio failed");
        return false;
    }
    if (version != kChipVersion) {
        *error = QStringLiteral("No RFM9x radio found (version register reads 0x%1)")
                     .arg(uint(version), 2, 16, QLatin1Char('0'));
        return false;
    }

    const bool lowBand = settings.frequencyHz < kLowBandMaxHz;
    m_modeBase = ModeLongRange | (lowBand ? ModeLowFrequency : 0);
    m_rssiOffset = lowBand ? -164 : -157;

    // LoRa mode can only be switched on while the chip is asleep.
    writeRegister(RegOpMode, ModeSleep);
    writeRegister(RegOpMode, m_modeBase | ModeSleep);
    if (!(readRegister(RegOpMode) & ModeLongRange)) {
        *error = QStringLiteral("Radio did not enter LoRa mode");
        return false;
    }

    const auto frf = static_cast<quint32>(std::llround(settings.frequencyHz * (1 << 19) / kOscillatorHz));
    writeRegister(RegFrfMsb, quint8(frf >> 16));
    writeRegister(RegFrfMid, quint8(frf >> 8));
    writeRegister(RegFrfLsb, quint8(frf));

    writeRegister(RegFifoRxBaseAddr, 0x00);

    writeRegister(RegModemConfig1, quint8(bandwidthCode(settings.bandwidthHz) << 4)
                                       | quint8((settings.codingRateDenominator - 4) << 1));
    writeRegister(RegModemConfig2, quint8(settings.spreadingFactor << 4) | RxPayloadCrcOn);

    // The datasheet requires this when one symbol lasts longer than 16 ms.
    const double symbolMs = (1 << settings.spreadingFactor) * 1000.0 / settings.bandwidthHz;
    writeRegister(RegModemConfig3, AgcAutoOn | (symbolMs > 16.0 ? LowDataRateOptimize : 0));

    writeRegister(RegPreambleMsb, quint8(settings.preambleLength >> 8));
    writeRegister(RegPreambleLsb, quint8(settings.preambleLength));
    writeRegister(RegSyncWord, settings.syncWord);
    writeRegister(RegDioMapping1, 0x00);
    writeRegister(RegIrqFlags, kClearAllIrqs);

    setMode(ModeStandby);
    setMode(ModeRxContinuous);

    if (m_busError) {
        *error = QStringLiteral("SPI transfer to the radio failed");
        return false;
    }
    return true;
}

Sx127x::Event Sx127x::poll(LoRaPacket *packet)
{
    const quint8 mode = readRegister(RegOpMode);
    const quint8 flags = readRegister(RegIrqFlags);
    if (m_busError)
        return Event::Fault;

    // Anything but continuous receive means the chip reset or lost config.
    if ((mode & kModeCheckMask) != (m_modeBase | ModeRxContinuous))
        return Event::Fault;

    if (!(flags & IrqRxDone))
        return Event::None;

    writeRegister(RegIrqFlags, kClearAllIrqs);
    if (flags & IrqPayloadCrcError)
        return Event::CrcError;

    const int length = readRegister(RegRxNbBytes);
    writeRegister(RegFifoAddrPtr, readRegister(RegFifoRxCurrentAddr));
    packet->payload = readFifo(length);

    packet->snrDb = static_cast<qint8>(readRegister(RegPktSnrValue)) / 4.0;
    packet->rssiDbm = m_rssiOffset + readRegister(RegPktRssiValue);
    if (packet->snrDb < 0.0)
        packet->rssiDbm += static_cast<int>(std::lround(packet->snrDb));

    return m_busError ? Event::Fault : Event::PacketReceived;
}

quint8 Sx127x::readRegister(quint8 reg)
{
    QByteArray buffer(2, '\0');
    buffer[0] = char(reg & ~kWriteBit);
    if (!m_bus->transfer(&buffer)) {
        m_busError = true;
        return 0;
    }
    return quint8(buffer[1]);
}

void Sx127x::writeRegister(quint8 reg, quint8 value)
{
    QByteArray buffer(2, '\0');
    buffer[0] = char(reg | kWriteBit);
    buffer[1] = char(value);
    if (!m_bus->transfer(&buffer))
        m_busError = true;
}

QByteArray Sx127x::readFifo(int length)
{
    QByteArray buffer(length + 1, '\0');
    buffer[0] = char(RegFifo);
    if (!m_bus->transfer(&buffer)) {
        m_busError = true;
        return {};
    }
    return buffer.mid(1);
}

void Sx127x::setMode(quint8 mode)
{
    writeRegister(RegOpMode, m_modeBase | mode);
}
