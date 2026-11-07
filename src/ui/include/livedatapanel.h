#ifndef LIVEDATAPANEL_H
#define LIVEDATAPANEL_H

/*
 * livedatapanel.h
 * Purpose:
 *   The LIVE DATA panel: a grid of tiles with the latest value of each
 *   reading.
 */

#include "retropanel.h"
#include "telemetry.h"

class QGridLayout;
class QLabel;

class LiveDataPanel : public RetroPanel
{
    Q_OBJECT

public:
    explicit LiveDataPanel(QWidget *parent = nullptr);

    void updateTelemetry(const Telemetry &telemetry);

private:
    QLabel *addTile(int row, int column, const QString &name);

    QGridLayout *m_grid;
    QLabel *m_altitude;
    QLabel *m_temperature;
    QLabel *m_pressure;
    QLabel *m_speed;
    QLabel *m_orientation;
    QLabel *m_tilt;
};

#endif // LIVEDATAPANEL_H
