#include "sprites.h"
#include "theme.h"

#include <QFile>
#include <QHash>
#include <QPainter>

namespace Sprites {

QPixmap get(const QString &name)
{
    static QHash<QString, QPixmap> cache;
    auto it = cache.find(name);
    if (it == cache.end())
        it = cache.insert(name, QPixmap(QStringLiteral(":/sprites/") + name + QStringLiteral(".png")));
    return *it;
}

void draw(QPainter &painter, const QRect &rect, const QString &name)
{
    painter.save();

    const QPixmap pixmap = get(name);
    if (!pixmap.isNull()) {
        // Enlarging: nearest-neighbour keeps pixel art crisp. Shrinking big
        // art: smooth, or thin outlines break up. Cached, since animated
        // scenes redraw the same sizes many times a second.
        static QHash<QString, QPixmap> scaledCache;
        const QString key = QStringLiteral("%1@%2x%3").arg(name).arg(rect.width()).arg(rect.height());
        auto it = scaledCache.find(key);
        if (it == scaledCache.end()) {
            const bool shrinking = pixmap.width() > rect.width() || pixmap.height() > rect.height();
            it = scaledCache.insert(key, pixmap.scaled(rect.size(), Qt::KeepAspectRatio,
                                                       shrinking ? Qt::SmoothTransformation
                                                                 : Qt::FastTransformation));
        }
        const QPixmap &scaled = *it;
        QRect target(QPoint(0, 0), scaled.size());
        target.moveCenter(rect.center());
        painter.drawPixmap(target.topLeft(), scaled);
    } else {
        const QColor magenta("#ff2bd6");
        painter.setPen(QPen(magenta, 2, Qt::DashLine));
        painter.setBrush(QColor(255, 43, 214, 40));
        painter.drawRect(rect.adjusted(1, 1, -1, -1));
        if (rect.width() >= 28) {
            painter.setPen(magenta);
            painter.setFont(Theme::pixelFont(qBound(7, rect.height() / 4, 10)));
            painter.drawText(rect, Qt::AlignCenter | Qt::TextWrapAnywhere, name.toUpper());
        }
    }

    painter.restore();
}

QString frameAt(const QString &name, qint64 timeMs, int fps, bool pingPong)
{
    static QHash<QString, int> frameCounts;
    auto it = frameCounts.find(name);
    if (it == frameCounts.end()) {
        int count = 0;
        while (QFile::exists(QStringLiteral(":/sprites/%1_%2.png").arg(name).arg(count)))
            ++count;
        it = frameCounts.insert(name, count);
    }
    const int count = *it;
    if (count == 0)
        return name;

    const qint64 step = timeMs * fps / 1000;
    int index = static_cast<int>(step % count);
    if (pingPong && count > 1) {
        const int period = 2 * (count - 1);
        const int position = static_cast<int>(step % period);
        index = position < count ? position : period - position;
    }
    return QStringLiteral("%1_%2").arg(name).arg(index);
}

} // namespace Sprites

SpriteSlot::SpriteSlot(const QString &name, const QSize &size, QWidget *parent)
    : QWidget(parent), m_name(name)
{
    setFixedSize(size);
}

void SpriteSlot::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    Sprites::draw(painter, rect(), m_name);
}
