#include "statusscreen.h"
#include "handheldshell.h"
#include "sprites.h"
#include "theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QProgressBar>
#include <QVBoxLayout>

namespace {

constexpr int kRssiMin = -120;  // dBm shown as an empty bar
constexpr int kRssiMax = -40;   // dBm shown as a full bar

// Unlit LCD segments show faintly, like on the real thing
const QColor kLcdGhost("#aab09c");

QLabel *makeLabel(const QString &text, int pixelSize, Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *label = new QLabel(text);
    label->setFont(Theme::pixelFont(pixelSize));
    label->setAlignment(align);
    return label;
}

} // namespace

// One LCD segment per flight state: lit up to and including the current one
class PhaseSegments : public QWidget
{
public:
    PhaseSegments() { setFixedHeight(14); }

    void setState(qint32 state)
    {
        m_state = state;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const int gap = 4;
        const int segment = (width() - gap * (kStateCount - 1)) / kStateCount;
        for (int i = 0; i < kStateCount; ++i)
            p.fillRect(i * (segment + gap), 0, segment, height(),
                       i <= static_cast<int>(m_state) ? Theme::Ink : kLcdGhost);
    }

private:
    qint32 m_state = StateIdle;
};

StatusScreen::StatusScreen(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_shell = new HandheldShell(QStringLiteral("METROCKETRY"), QStringLiteral("GAME & WATCH"));
    outer->addWidget(m_shell);

    // LCD style: black on grey-green, signal bar drawn as black segments
    QWidget *lcd = m_shell->screen();
    lcd->setStyleSheet(QStringLiteral(R"(
        QLabel { color: %1; }
        QLabel#inverse { background: %1; color: %2; padding: 0px 4px; }
        QProgressBar { background: %3; border: 2px solid %1; }
        QProgressBar::chunk { background: %1; width: 6px; margin: 1px; }
    )").arg(Theme::Ink.name(), Theme::Lcd.name(), kLcdGhost.name()));

    auto *grid = new QGridLayout(lcd);
    grid->setContentsMargins(6, 4, 6, 4);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(2);

    auto *identity = new QVBoxLayout;
    identity->setSpacing(1);
    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(makeLabel(QStringLiteral("STOP DROP N' ROLL"), 15));
    nameRow->addStretch();
    m_source = makeLabel(QString(), 11);
    nameRow->addWidget(m_source);
    m_phase = makeLabel(QString(), 12);
    m_phase->setObjectName(QStringLiteral("inverse"));
    m_phase->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_packets = makeLabel(QString(), 12);
    identity->addLayout(nameRow);
    identity->addWidget(m_phase);
    identity->addWidget(m_packets);

    // The rocket's photo stands in a tall column down the left of the LCD
    grid->addWidget(new SpriteSlot(Sprites::Avatar, QSize(28, 140)), 0, 0, 6, 1, Qt::AlignCenter);
    grid->addLayout(identity, 0, 1, 1, 3);

    m_signal = new QProgressBar;
    m_signal->setRange(kRssiMin, kRssiMax);
    m_signal->setValue(kRssiMin);
    m_signal->setTextVisible(false);
    m_signal->setFixedHeight(12);
    m_rssi = makeLabel(QString(), 12, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(makeLabel(QStringLiteral("SG"), 12), 1, 1);
    grid->addWidget(m_signal, 1, 2);
    grid->addWidget(m_rssi, 1, 3);

    m_pressure = makeLabel(QString(), 12, Qt::AlignRight | Qt::AlignVCenter);
    m_gps = makeLabel(QString(), 12, Qt::AlignRight | Qt::AlignVCenter);
    m_logging = makeLabel(QString(), 12, Qt::AlignRight | Qt::AlignVCenter);
    grid->addWidget(makeLabel(QStringLiteral("▸ PRESSURE"), 11), 2, 1, 1, 2);
    grid->addWidget(m_pressure, 2, 3);
    grid->addWidget(makeLabel(QStringLiteral("▸ GPS"), 11), 3, 1, 1, 2);
    grid->addWidget(m_gps, 3, 3);
    grid->addWidget(makeLabel(QStringLiteral("▸ LOGGING"), 11), 4, 1, 1, 2);
    grid->addWidget(m_logging, 4, 3);

    m_segments = new PhaseSegments;
    grid->addWidget(m_segments, 5, 1, 1, 3);

    // Fixed value column so changing numbers don't shift the bar
    grid->setColumnMinimumWidth(3, QFontMetrics(Theme::pixelFont(12)).horizontalAdvance(QStringLiteral("101.3 kPa")));
    grid->setColumnStretch(2, 1);
    grid->setRowStretch(5, 1);

    setLogging(false);
    updateTelemetry(Telemetry{});
}

void StatusScreen::updateTelemetry(const Telemetry &telemetry)
{
    m_phase->setText(flightStateName(telemetry.flightState));
    m_packets->setText(QStringLiteral("PKTS %1").arg(telemetry.packetsReceived));
    m_pressure->setText(QStringLiteral("%1 kPa").arg(telemetry.pressure, 0, 'f', 1));
    m_gps->setText(telemetry.gpsHasFix ? QStringLiteral("FIX") : QStringLiteral("NO FIX"));
    m_segments->setState(telemetry.flightState);
    if (telemetry.packetsReceived > 0)
        m_shell->flashLight();
}

void StatusScreen::setSignal(int rssiDbm)
{
    m_signal->setValue(qBound(kRssiMin, rssiDbm, kRssiMax));
    m_rssi->setText(QString::number(rssiDbm));
}

void StatusScreen::setLinkLabel(const QString &linkLabel)
{
    m_source->setText(linkLabel);
}

void StatusScreen::setLogging(bool logging)
{
    m_logging->setText(logging ? QStringLiteral("● REC") : QStringLiteral("OFF"));
}
