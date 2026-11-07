#include "gpsmap.h"
#include "sprites.h"
#include "theme.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {

const QString kMapFile = QStringLiteral(":/maps/urrg.json");

// Tracker palette
const QColor kFrame("#5bb8e0");
const QColor kFrameDark("#2f86b4");
const QColor kMapBg("#0e2747");
const QColor kWater("#071a33");
const QColor kRoadMinor("#24507c");
const QColor kRoadLocal("#3a73a8");
const QColor kRoadMajor("#5b97cf");
const QColor kRoadCore("#8cc0ea");
const QColor kHud("#7fc6ef");
const QColor kPlaque("#f7963b");
const QColor kTab("#e8413c");
const QColor kTrack("#ff7a3c");

constexpr int kButtonRow = 30;   // px for the button tray under the map
constexpr int kHeadroom = 28;    // px of frame above the map, holding the mascot head and pods
constexpr double kFitMin = 400;  // m from centre to edge, FIT view
constexpr double kFollowRange = 300;
constexpr double kAreaRange = 2500;

QPolygonF readPoints(const QJsonArray &flat, const std::function<QPointF(double, double)> &project)
{
    QPolygonF points;
    for (int i = 0; i + 1 < flat.size(); i += 2)
        points << project(flat[i].toDouble(), flat[i + 1].toDouble());
    return points;
}

QString formatCoordinate(double degrees, char positive, char negative)
{
    return QStringLiteral("%1%2").arg(std::abs(degrees), 0, 'f', 4).arg(QLatin1Char(degrees >= 0 ? positive : negative));
}

// Capsule with a number in it, like the digit pods by the mascot head
void drawPod(QPainter &p, const QRect &rect, const QString &text)
{
    p.setPen(QPen(Theme::Ink, 2));
    p.setBrush(QColor("#f4f8fb"));
    p.drawRoundedRect(rect, rect.height() / 2.0, rect.height() / 2.0);
    p.setPen(Theme::Ink);
    p.setFont(Theme::pixelFont(10));
    p.drawText(rect, Qt::AlignCenter, text);
}

// Red side tab with a letter, sticking out of the frame's left edge
void drawTab(QPainter &p, const QRect &rect, const QString &letter)
{
    QPolygon shape;
    shape << rect.topLeft() << QPoint(rect.right() - 5, rect.top()) << QPoint(rect.right(), rect.center().y())
          << QPoint(rect.right() - 5, rect.bottom()) << rect.bottomLeft();
    p.setPen(QPen(Theme::Ink, 2));
    p.setBrush(kTab);
    p.drawPolygon(shape);
    p.setPen(Theme::Ink);
    p.setFont(Theme::pixelFont(12));
    p.drawText(rect.adjusted(0, 0, -4, 0), Qt::AlignCenter, letter);
}

// Pill button in the tray. Painted only: the tray is for looks, the
// physical buttons do the work. lit marks the current view.
void drawButton(QPainter &p, const QRect &rect, const QString &label, const QColor &color, bool lit)
{
    p.setPen(QPen(Theme::Ink, 2));
    p.setBrush(lit ? color.lighter(135) : color);
    p.drawRoundedRect(rect, 6, 6);
    p.setPen(Theme::Ink);
    p.setFont(Theme::pixelFont(10));
    p.drawText(rect, Qt::AlignCenter, label);
}

} // namespace

GpsMap::GpsMap(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 200);
    loadMap();
}

void GpsMap::loadMap()
{
    QFile file(kMapFile);
    if (!file.open(QIODevice::ReadOnly))
        return;  // no map file: the tracker still works on a blank map
    const QJsonObject map = QJsonDocument::fromJson(file.readAll()).object();

    const QJsonArray origin = map.value(QStringLiteral("origin")).toArray();
    m_originLatitude = origin.at(0).toDouble();
    m_originLongitude = origin.at(1).toDouble();
    m_siteName = map.value(QStringLiteral("name")).toString();
    m_attribution = map.value(QStringLiteral("attribution")).toString();

    auto project = [this](double latitude, double longitude) { return toMetres(latitude, longitude); };
    for (const QJsonValue &value : map.value(QStringLiteral("roads")).toArray()) {
        const QJsonObject road = value.toObject();
        m_roads.append({road.value(QStringLiteral("c")).toInt(), readPoints(road.value(QStringLiteral("p")).toArray(), project)});
    }
    // Draw the small roads first so the big ones sit on top
    std::stable_sort(m_roads.begin(), m_roads.end(), [](const Road &a, const Road &b) { return a.rank < b.rank; });

    for (const QJsonValue &value : map.value(QStringLiteral("water")).toArray())
        m_water.append(readPoints(value.toObject().value(QStringLiteral("p")).toArray(), project));
    for (const QJsonValue &value : map.value(QStringLiteral("streams")).toArray())
        m_streams.append(readPoints(value.toObject().value(QStringLiteral("p")).toArray(), project));
    for (const QJsonValue &value : map.value(QStringLiteral("places")).toArray()) {
        const QJsonObject place = value.toObject();
        const QJsonArray at = place.value(QStringLiteral("p")).toArray();
        m_places.append({place.value(QStringLiteral("n")).toString().toUpper(),
                         toMetres(at.at(0).toDouble(), at.at(1).toDouble())});
    }
}

// Flat-earth approximation, fine over a few km
QPointF GpsMap::toMetres(double latitude, double longitude) const
{
    return QPointF((longitude - m_originLongitude) * 111320.0 * std::cos(qDegreesToRadians(m_originLatitude)),
                   (latitude - m_originLatitude) * 110540.0);
}

QRect GpsMap::mapRect() const
{
    // Left margin leaves room for the G P S tabs inside the frame
    return QRect(30, kHeadroom, width() - 38, height() - kHeadroom - kButtonRow - 8);
}

void GpsMap::updateTelemetry(const Telemetry &telemetry)
{
    m_hasFix = telemetry.gpsHasFix;
    if (!m_hasFix) {
        update();
        return;
    }

    const QPointF here = toMetres(telemetry.gpsLatitude, telemetry.gpsLongitude);
    if (!m_hasPad) {
        m_pad = here;
        m_hasPad = true;
    }
    m_track.append(here);
    m_latitude = telemetry.gpsLatitude;
    m_longitude = telemetry.gpsLongitude;
    update();
}

void GpsMap::setView(View view)
{
    m_view = view;
    update();
}

void GpsMap::clear()
{
    m_hasPad = false;
    m_track.clear();
    update();
}

void GpsMap::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Frame: light blue body with the map window and a button tray
    // Everything below is drawn inside this frame; nothing hangs outside it
    const QRect body = rect().adjusted(1, 1, -2, -2);
    p.setPen(QPen(Theme::Ink, 3));
    p.setBrush(kFrame);
    p.drawRoundedRect(body, 6, 6);

    const QRect map = mapRect();

    // Where the map is looking, and how far it shows
    QPointF centre = m_hasPad ? m_pad : QPointF(0, 0);
    double range = kFitMin;
    const QPointF rocket = m_track.isEmpty() ? centre : m_track.last();
    switch (m_view) {
    case View::Fit:
        if (!m_track.isEmpty()) {
            centre = (m_pad + rocket) / 2;
            const QPointF span = rocket - m_pad;
            range = std::max(kFitMin, std::max(std::abs(span.x()), std::abs(span.y())) / 2 * 1.4);
        }
        break;
    case View::Follow:
        centre = rocket;
        range = kFollowRange;
        break;
    case View::Area:
        range = kAreaRange;
        break;
    }
    const double scale = std::min(map.width(), map.height()) / 2.0 / range;  // px per metre
    auto toScreen = [&](const QPointF &metres) {
        return QPointF(map.center().x() + (metres.x() - centre.x()) * scale,
                       map.center().y() - (metres.y() - centre.y()) * scale);
    };
    auto toScreenPolygon = [&](const QPolygonF &metres) {
        QPolygonF screen;
        screen.reserve(metres.size());
        for (const QPointF &point : metres)
            screen << toScreen(point);
        return screen;
    };

    // Map
    p.save();
    p.setClipRect(map);
    p.fillRect(map, kMapBg);

    // Faint survey grid every 100 m (every 500 m when zoomed out)
    const double gridStep = range > 1000 ? 500 : 100;
    p.setPen(QPen(QColor(127, 198, 239, 22), 1));
    const double left = centre.x() - map.width() / 2.0 / scale;
    const double bottom = centre.y() - map.height() / 2.0 / scale;
    for (double x = std::floor(left / gridStep) * gridStep; toScreen({x, 0}).x() < map.right(); x += gridStep)
        p.drawLine(QPointF(toScreen({x, 0}).x(), map.top()), QPointF(toScreen({x, 0}).x(), map.bottom()));
    for (double y = std::floor(bottom / gridStep) * gridStep; toScreen({0, y}).y() > map.top(); y += gridStep)
        p.drawLine(QPointF(map.left(), toScreen({0, y}).y()), QPointF(map.right(), toScreen({0, y}).y()));

    p.setPen(Qt::NoPen);
    p.setBrush(kWater);
    for (const QPolygonF &water : m_water)
        p.drawPolygon(toScreenPolygon(water));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kWater.lighter(160), 2));
    for (const QPolygonF &stream : m_streams)
        p.drawPolyline(toScreenPolygon(stream));

    // Roads: outline then a lighter core on the big ones, like the reference
    const double zoom = std::clamp(scale * 4, 0.6, 1.6);  // thinner when zoomed out
    for (const Road &road : m_roads) {
        const QPolygonF line = toScreenPolygon(road.points);
        switch (road.rank) {
        case 0: p.setPen(QPen(kRoadMinor, 2 * zoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); break;
        case 1: p.setPen(QPen(kRoadLocal, 4 * zoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); break;
        default: p.setPen(QPen(kRoadMajor, (road.rank == 3 ? 8 : 6) * zoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); break;
        }
        p.drawPolyline(line);
        if (road.rank >= 2) {
            p.setPen(QPen(kRoadCore, 1.5 * zoom, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.drawPolyline(line);
        }
    }

    // Place names: the site in big letters, hamlets smaller
    p.setPen(QColor(200, 220, 230, 150));
    p.setFont(Theme::pixelFont(26));
    const QPointF site = toScreen({0, 0});
    p.drawText(QRectF(site.x() - 100, site.y() + 10, 200, 30), Qt::AlignCenter, m_siteName);
    p.setFont(Theme::pixelFont(11));
    p.setPen(QColor(200, 220, 230, 170));
    for (const Place &place : m_places) {
        const QPointF at = toScreen(place.position);
        if (map.contains(at.toPoint()))
            p.drawText(QRectF(at.x() - 60, at.y() - 8, 120, 16), Qt::AlignCenter, place.name);
    }

    // Track and pad
    if (m_track.size() > 1) {
        p.setPen(QPen(kTrack, 2.5, Qt::DashLine, Qt::RoundCap));
        p.drawPolyline(toScreenPolygon(QPolygonF(m_track)));
    }
    if (m_hasPad) {
        const QPointF pad = toScreen(m_pad);
        p.setPen(QPen(Theme::Ink, 2));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(pad.x() - 5, pad.y() - 5, 10, 10));
        p.setPen(Qt::white);
        p.setFont(Theme::pixelFont(10));
        p.drawText(QRectF(pad.x() - 20, pad.y() + 6, 40, 12), Qt::AlignCenter, QStringLiteral("PAD"));
    }

    // Rocket: red disc with the marker sprite in it
    if (!m_track.isEmpty()) {
        const QPointF at = toScreen(rocket);
        p.setPen(QPen(Theme::Ink, 2));
        p.setBrush(kTab);
        p.drawEllipse(at, 12, 12);
        Sprites::draw(p, QRect(static_cast<int>(at.x()) - 8, static_cast<int>(at.y()) - 8, 16, 16), Sprites::Marker);
    }

    // HUD: reticle and satellite count top-left, ruler down the left edge
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kHud, 1.5));
    const QPointF reticle(map.left() + 18, map.bottom() - 30);
    p.drawEllipse(reticle, 9, 9);
    p.drawEllipse(reticle, 4, 4);
    p.drawLine(reticle + QPointF(-13, 0), reticle + QPointF(13, 0));
    p.drawLine(reticle + QPointF(0, -13), reticle + QPointF(0, 13));
    p.setFont(Theme::pixelFont(10));
    p.drawText(QRectF(reticle.x() + 14, reticle.y() - 12, 80, 12), Qt::AlignLeft | Qt::AlignVCenter,
               m_hasFix ? QStringLiteral("GPS FIX") : QStringLiteral("NO FIX"));
    p.fillRect(QRectF(reticle.x() + 14, reticle.y() + 2, 50, 3), QColor(127, 198, 239, 60));
    p.fillRect(QRectF(reticle.x() + 14, reticle.y() + 2, m_hasFix ? 50 : 0, 3), kHud);
    for (int y = map.top() + 40; y < map.bottom() - 46; y += 8)
        p.drawLine(QPointF(map.left() + 2, y), QPointF(map.left() + (y % 40 == 0 ? 9 : 5), y));

    // Scale bar and the attribution the OpenStreetMap licence asks for
    const double barMetres = range > 1000 ? 1000 : (range > 300 ? 200 : 100);
    const QPointF barStart(map.right() - 8 - barMetres * scale, map.bottom() - 18);
    p.setPen(QPen(kHud, 2));
    p.drawLine(barStart, barStart + QPointF(barMetres * scale, 0));
    p.drawText(QRectF(barStart.x(), barStart.y() - 13, barMetres * scale, 12), Qt::AlignCenter,
               barMetres >= 1000 ? QStringLiteral("1 km") : QStringLiteral("%1 m").arg(barMetres));
    p.setPen(QColor(200, 220, 230, 130));
    p.setFont(Theme::pixelFont(8, false));
    const QRect credit = map.adjusted(4, 0, -4, -2);
    p.drawText(credit, Qt::AlignRight | Qt::AlignBottom,
               p.fontMetrics().elidedText(m_attribution, Qt::ElideRight, credit.width()));
    p.restore();

    // Map window border
    p.setPen(QPen(kFrameDark, 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(map);

    // Top-right icon box
    const QRect icon(map.right() - 32, 5, 32, 32);
    p.setPen(QPen(Theme::Ink, 2));
    p.setBrush(kPlaque);
    p.drawRoundedRect(icon, 5, 5);
    Sprites::draw(p, icon.adjusted(4, 4, -4, -4), Sprites::TrackerIcon);

    // Plaque: what we're tracking and where, with the mascot on top and
    // distance / bearing pods either side of it
    const bool fix = !m_track.isEmpty();
    const QPointF offset = fix ? rocket - m_pad : QPointF();
    const double distance = std::hypot(offset.x(), offset.y());
    double bearing = qRadiansToDegrees(std::atan2(offset.x(), offset.y()));
    if (bearing < 0)
        bearing += 360;

    const int plaqueWidth = std::min(map.width() - 70, 190);
    QRect plaque(0, map.top() + 4, plaqueWidth, 34);
    plaque.moveLeft(map.center().x() - plaqueWidth / 2 - 14);
    p.setPen(QPen(Theme::Ink, 2));
    p.setBrush(kPlaque);
    p.drawRoundedRect(plaque, 4, 4);
    p.setPen(Theme::Ink);
    p.setFont(Theme::pixelFont(11));
    p.drawText(plaque.adjusted(4, 2, -4, -17), Qt::AlignCenter, QStringLiteral("G.P.S TRACKER"));
    p.setFont(Theme::pixelFont(10));
    p.drawText(plaque.adjusted(4, 16, -4, -2), Qt::AlignCenter,
               fix ? formatCoordinate(m_latitude, 'N', 'S') + QStringLiteral(" ") + formatCoordinate(m_longitude, 'E', 'W')
                   : QStringLiteral("LOCATION: ????"));

    const int headSize = 26;
    const QRect head(plaque.center().x() - headSize / 2, plaque.top() - headSize + 4, headSize, headSize);
    drawPod(p, QRect(head.left() - 52, head.top() + 4, 48, 15), fix ? QStringLiteral("%1m").arg(qRound(distance)) : QStringLiteral("----"));
    drawPod(p, QRect(head.right() + 4, head.top() + 4, 48, 15),
            fix ? QStringLiteral("%1°").arg(qRound(bearing) % 360, 3, 10, QLatin1Char('0')) : QStringLiteral("---°"));
    Sprites::draw(p, head, Sprites::TrackerHead);

    // Side tabs and the buddy in the corner
    p.setRenderHint(QPainter::Antialiasing, false);
    const int tabTop = map.top() + map.height() / 2 - 40;
    const QStringList letters = {QStringLiteral("G"), QStringLiteral("P"), QStringLiteral("S")};
    for (int i = 0; i < letters.size(); ++i)
        drawTab(p, QRect(5, tabTop + i * 26, 22, 22), letters[i]);
    Sprites::draw(p, QRect(6, height() - kButtonRow - 2, 22, 22), Sprites::TrackerBuddy);

    // Button tray, like the reference tracker
    p.setRenderHint(QPainter::Antialiasing);
    const QList<QPair<QString, View>> buttons = {
        {QStringLiteral("FIT"), View::Fit},
        {QStringLiteral("FOLLOW"), View::Follow},
        {QStringLiteral("AREA"), View::Area},
    };
    const int trayLeft = 32;
    const int gap = 5;
    const int buttonWidth = (width() - trayLeft - 8 - gap * (buttons.size() - 1)) / buttons.size();
    for (int i = 0; i < buttons.size(); ++i) {
        const QRect button(trayLeft + i * (buttonWidth + gap), height() - kButtonRow - 2, buttonWidth, 22);
        drawButton(p, button, buttons[i].first, buttons[i].second == View::Area ? kPlaque : QColor("#7be08a"),
                   buttons[i].second == m_view);
    }
}
