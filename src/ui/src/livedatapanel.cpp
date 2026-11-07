#include "livedatapanel.h"
#include "theme.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

LiveDataPanel::LiveDataPanel(QWidget *parent)
    : RetroPanel(QStringLiteral("LIVE DATA"), Theme::LightBlue, parent)
{
    m_grid = new QGridLayout;
    m_grid->setSpacing(6);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    body()->addLayout(m_grid, 1);

    m_altitude = addTile(0, 0, QStringLiteral("ALT m"));
    m_temperature = addTile(0, 1, QStringLiteral("TEMP °C"));
    m_pressure = addTile(1, 0, QStringLiteral("PRESS kPa"));
    m_speed = addTile(1, 1, QStringLiteral("SPEED m/s"));
    m_orientation = addTile(2, 0, QStringLiteral("R P Y °"));
    m_tilt = addTile(2, 1, QStringLiteral("TILT °"));

    updateTelemetry(Telemetry{});
}

QLabel *LiveDataPanel::addTile(int row, int column, const QString &name)
{
    auto *tile = new QFrame;
    tile->setObjectName(QStringLiteral("tile"));
    // Equal columns no matter how wide the numbers get
    tile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(tile);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(0);

    auto *nameLabel = new QLabel(name);
    nameLabel->setFont(Theme::pixelFont(11));
    auto *value = new QLabel;
    value->setFont(Theme::pixelFont(17));
    value->setStyleSheet(QStringLiteral("color: %1;").arg(Theme::Blue.name()));

    layout->addWidget(nameLabel);
    layout->addWidget(value);
    m_grid->addWidget(tile, row, column);
    return value;
}

void LiveDataPanel::updateTelemetry(const Telemetry &telemetry)
{
    m_altitude->setText(QString::number(telemetry.altitude, 'f', 1));
    m_temperature->setText(QString::number(telemetry.temperature, 'f', 1));
    m_pressure->setText(QString::number(telemetry.pressure, 'f', 1));
    m_speed->setText(QString::number(telemetry.velocity, 'f', 1));
    m_orientation->setText(QStringLiteral("%1 %2 %3")
                               .arg(qRound(telemetry.roll))
                               .arg(qRound(telemetry.pitch))
                               .arg(qRound(telemetry.yaw)));
    m_tilt->setText(QString::number(telemetry.tiltAngle, 'f', 1));
    setHeaderInfo(QStringLiteral("#%1").arg(telemetry.packetsReceived));
}
