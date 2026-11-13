#ifndef ALTITUDESCENE_H
#define ALTITUDESCENE_H

/*
 * altitudescene.h
 * Purpose:
 *   The Kirby-style sky view in the middle of the dashboard. Kirby sits
 *   at the rocket's current altitude: standing on the pad, puffed up and
 *   floating on the way up, drifting down under his parasol in Descent,
 *   and standing again once landed. The starry sky
 *   scrolls past him in the opposite direction to his vertical speed, so
 *   the scene moves while he does. A dotted trail shows the flight so
 *   far, a dashed line marks the highest point, and a status bar along
 *   the top shows what's happening and the last event. When the packet
 *   carries no altitude the bar says so and nothing is plotted.
 */

#include "telemetry.h"

#include <QElapsedTimer>
#include <QPointF>
#include <QVector>
#include <QWidget>

class QTimer;

class AltitudeScene : public QWidget
{
    Q_OBJECT

public:
    explicit AltitudeScene(QWidget *parent = nullptr);

    void addSample(const Telemetry &telemetry);
    void setLastEvent(const QString &text);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void animate();

    QVector<QPointF> m_trail;  // (s since launch, altitude m)
    double m_launchTime = 0;   // mission time of the last sample on the pad
    double m_maxAltitude = 0;
    double m_speed = 0;
    qint32 m_state = StateIdle;
    bool m_onGround = true;
    bool m_hasAltitude = true; // false once a packet arrives without it
    QString m_lastEvent;

    // Animation: runs continuously, independent of the packet rate
    QTimer *m_animation;
    QElapsedTimer m_clock;
    qint64 m_lastFrameMs = 0;
    double m_scroll = 0;       // px the sky has scrolled; grows while rising
};

#endif // ALTITUDESCENE_H
