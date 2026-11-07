#ifndef SPRITES_H
#define SPRITES_H

/*
 * sprites.h
 * Purpose:
 *   Character images (Pac-Man, Mario, ...). Each sprite has a name; put
 *   a PNG called <name>.png in assets/sprites/ and rebuild, and it
 *   replaces the pink placeholder box everywhere that name is drawn.
 *
 *   Painted widgets call Sprites::draw(); layouts use a SpriteSlot.
 *   Animations are numbered frames: name_0.png, name_1.png, ...
 */

#include <QPixmap>
#include <QString>
#include <QWidget>

class QPainter;

namespace Sprites {

inline const QString Logo   = QStringLiteral("logo");    // header, top-left
inline const QString Runner = QStringLiteral("runner");  // Pac-Man on the phase track
inline const QString Rocket = QStringLiteral("rocket");  // flies in the altitude scene
inline const QString Avatar = QStringLiteral("avatar");  // photo of the rocket on the status LCD
inline const QString Marker = QStringLiteral("marker");  // inside the red rocket disc on the GPS tracker

// One ghost per flight state on the phase track; Dead replaces a state once it's passed
inline const QString GhostRed    = QStringLiteral("ghost_red");
inline const QString GhostPink   = QStringLiteral("ghost_pink");
inline const QString GhostCyan   = QStringLiteral("ghost_cyan");
inline const QString GhostOrange = QStringLiteral("ghost_orange");
inline const QString GhostBlue   = QStringLiteral("ghost_blue");
inline const QString GhostDead   = QStringLiteral("ghost_dead");

// Graph critters: bird at the head of each line in flight, dog at each phase change
inline const QString Bird = QStringLiteral("bird");
inline const QString Dog  = QStringLiteral("dog");

// Kirby in the altitude scene, animated (kirby_float_0.png, kirby_float_1.png, ...):
// standing on the pad and after landing, puffed up and floating on the way
// up, holding a parasol on the way down
inline const QString KirbyGround  = QStringLiteral("kirby_ground");
inline const QString KirbyFloat   = QStringLiteral("kirby_float");
inline const QString KirbyParasol = QStringLiteral("kirby_parasol");

// GPS tracker: mascot head on the plaque, icon in the top-right box, buddy bottom-left
inline const QString TrackerHead  = QStringLiteral("tracker_head");
inline const QString TrackerIcon  = QStringLiteral("tracker_icon");
inline const QString TrackerBuddy = QStringLiteral("tracker_buddy");

// The image for name, or a null pixmap if there is no PNG for it yet
QPixmap get(const QString &name);

// Draws the sprite scaled to fit rect, or a placeholder if it's missing
void draw(QPainter &painter, const QRect &rect, const QString &name);

// For an animation saved as name_0.png, name_1.png, ...: the frame to show
// at timeMs. pingPong plays it forwards then backwards (for a swing).
// Returns name itself when there are no numbered frames.
QString frameAt(const QString &name, qint64 timeMs, int fps, bool pingPong = false);

} // namespace Sprites

class SpriteSlot : public QWidget
{
    Q_OBJECT

public:
    SpriteSlot(const QString &name, const QSize &size, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_name;
};

#endif // SPRITES_H
