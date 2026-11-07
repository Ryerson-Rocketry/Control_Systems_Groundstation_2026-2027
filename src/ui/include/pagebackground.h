#ifndef PAGEBACKGROUND_H
#define PAGEBACKGROUND_H

/*
 * pagebackground.h
 * Purpose:
 *   The widget behind the whole dashboard. Draws
 *   assets/sprites/background.png (Peach's castle) scaled to cover the
 *   window, cropping the edges rather than stretching it. If the image is
 *   missing it draws a pixel-art synthwave sunset in code instead.
 */

#include <QPixmap>
#include <QWidget>

class PageBackground : public QWidget
{
    Q_OBJECT

public:
    explicit PageBackground(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_scaled;   // background at the current window size
    QSize m_scaledFor;  // window size m_scaled was made for
};

#endif // PAGEBACKGROUND_H
