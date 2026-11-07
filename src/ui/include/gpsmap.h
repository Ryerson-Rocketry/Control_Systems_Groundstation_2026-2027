#ifndef GPSMAP_H
#define GPSMAP_H

/*
 * gpsmap.h
 * Purpose:
 *   The GPS tracker, styled like a superhero sighting tracker: a dark
 *   street map of the launch site (URRG, near Penn Yan NY) inside a light
 *   blue frame, an orange "G.P.S TRACKER" plaque with the location, readouts
 *   for distance and bearing from the pad, and the rocket's track.
 *
 *   The streets come from assets/maps/urrg.json (OpenStreetMap data,
 *   compiled in), so the map works with no internet. The pad is wherever
 *   the first GPS fix was.
 *
 *   setView() picks what the map shows: Fit (pad and rocket both in
 *   view, the default), Follow (centred on the rocket, close up) or Area
 *   (the wider area). The FIT / FOLLOW / AREA tray under the map is
 *   painted decoration that lights up the current view; it doesn't take
 *   touches. Hook setView() up to the physical buttons.
 */

#include "telemetry.h"

#include <QPointF>
#include <QVector>
#include <QWidget>

class GpsMap : public QWidget
{
    Q_OBJECT

public:
    enum class View { Fit, Follow, Area };

    explicit GpsMap(QWidget *parent = nullptr);

    void updateTelemetry(const Telemetry &telemetry);
    void clear();
    void setView(View view);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Road
    {
        int rank;  // 0 track/service, 1 local, 2 county, 3 state route
        QPolygonF points;  // metres east/north of the map origin
    };

    struct Place
    {
        QString name;
        QPointF position;  // metres east/north of the map origin
    };

    void loadMap();
    QPointF toMetres(double latitude, double longitude) const;
    QRect mapRect() const;

    // Map data, in metres from the origin
    double m_originLatitude = 0;
    double m_originLongitude = 0;
    QString m_siteName;
    QString m_attribution;
    QVector<Road> m_roads;
    QVector<QPolygonF> m_water;
    QVector<QPolygonF> m_streams;
    QVector<Place> m_places;

    // Flight
    bool m_hasPad = false;
    QPointF m_pad;
    QVector<QPointF> m_track;
    double m_latitude = 0;
    double m_longitude = 0;
    bool m_hasFix = false;

    View m_view = View::Fit;
};

#endif // GPSMAP_H
