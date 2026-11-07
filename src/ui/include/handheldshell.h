#ifndef HANDHELDSHELL_H
#define HANDHELDSHELL_H

/*
 * handheldshell.h
 * Purpose:
 *   A small Nintendo Game & Watch style handheld: white rim, blue
 *   faceplate, a white bezel round an LCD with the title above it and the
 *   Nintendo logo and caption below it, a D-pad on the left and A/B
 *   buttons on the right. Whatever goes in screen() shows on the LCD; the buttons are
 *   decoration only. Used as the body of the StatusScreen.
 */

#include <QWidget>

class QTimer;

class HandheldShell : public QWidget
{
    Q_OBJECT

public:
    HandheldShell(const QString &title, const QString &caption, QWidget *parent = nullptr);

    // Put the LCD's layout on this widget
    QWidget *screen() const { return m_screen; }

    // Blinks the little light above the buttons, e.g. once per packet
    void flashLight();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QRect bezelRect() const;
    QRect lcdRect() const;

    QString m_title;
    QString m_caption;
    QWidget *m_screen;
    QTimer *m_lightTimer;
    bool m_light = false;
};

#endif // HANDHELDSHELL_H
