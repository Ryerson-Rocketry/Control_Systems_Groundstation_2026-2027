#include "headerbar.h"
#include "sprites.h"
#include "theme.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr int kStatusWidth = 240;  // px
} // namespace

HeaderBar::HeaderBar(QWidget *parent) : QWidget(parent)
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);

    // Left: logo and mission clock
    row->addWidget(new SpriteSlot(Sprites::Logo, QSize(36, 36)));

    auto *left = new QVBoxLayout;
    left->setSpacing(0);
    m_missionTime = new QLabel;
    m_missionTime->setFont(Theme::pixelFont(13));
    m_status = new QLabel;
    m_status->setFont(Theme::pixelFont(10));
    m_status->setFixedWidth(kStatusWidth);  // long messages mustn't push the title
    left->addWidget(m_missionTime);
    left->addWidget(m_status);
    row->addLayout(left);

    // Middle: title with a hard pixel drop shadow
    row->addStretch();
    auto *title = new QLabel(QStringLiteral("GROUNDSTATION 2027"));
    title->setFont(Theme::pixelFont(28));
    title->setStyleSheet(QStringLiteral("color: white;"));
    auto *shadow = new QGraphicsDropShadowEffect(title);
    shadow->setBlurRadius(0);
    shadow->setOffset(3, 3);
    shadow->setColor(Theme::Ink);
    title->setGraphicsEffect(shadow);
    row->addWidget(title);
    row->addStretch();

    // Right: packet counts and mode
    auto *right = new QVBoxLayout;
    right->setSpacing(0);
    m_received = new QLabel;
    m_received->setFont(Theme::pixelFont(13));
    m_received->setAlignment(Qt::AlignRight);
    m_lost = new QLabel;
    m_lost->setFont(Theme::pixelFont(13));
    m_lost->setAlignment(Qt::AlignRight);
    right->addWidget(m_received);
    right->addWidget(m_lost);
    row->addLayout(right);

    auto *modeBox = new QFrame;
    modeBox->setObjectName(QStringLiteral("modeBox"));
    modeBox->setFixedSize(48, 42);
    auto *modeLayout = new QVBoxLayout(modeBox);
    modeLayout->setContentsMargins(0, 2, 0, 2);
    modeLayout->setSpacing(0);
    auto *modeTitle = new QLabel(QStringLiteral("MODE"));
    modeTitle->setFont(Theme::pixelFont(9));
    modeTitle->setAlignment(Qt::AlignCenter);
    m_mode = new QLabel;
    m_mode->setFont(Theme::pixelFont(18));
    m_mode->setAlignment(Qt::AlignCenter);
    modeLayout->addWidget(modeTitle);
    modeLayout->addWidget(m_mode);
    row->addWidget(modeBox);

    updateTelemetry(Telemetry{});
}

void HeaderBar::setStatus(const QString &text)
{
    m_status->setText(m_status->fontMetrics().elidedText(text, Qt::ElideRight, kStatusWidth));
}

void HeaderBar::setLost(quint64 lost)
{
    m_lost->setText(QStringLiteral("LOST %1").arg(lost));
}

void HeaderBar::updateTelemetry(const Telemetry &telemetry)
{
    m_missionTime->setText(QStringLiteral("MISSION T+ ") + formatMissionTime(telemetry.missionTime, true));
    m_received->setText(QStringLiteral("RX %1").arg(telemetry.packetsReceived));
    setLost(telemetry.packetsLost);
    // S = simulated data, F = flight (real radio)
    m_mode->setText(telemetry.simulated ? QStringLiteral("S") : QStringLiteral("F"));
}
