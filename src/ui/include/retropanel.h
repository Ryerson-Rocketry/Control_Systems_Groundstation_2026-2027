#ifndef RETROPANEL_H
#define RETROPANEL_H

/*
 * retropanel.h
 * Purpose:
 *   The thick-bordered box with a coloured title strip used by the
 *   LIVE DATA and GPS POSITION panels. Put content in body().
 */

#include <QFrame>

class QLabel;
class QVBoxLayout;

class RetroPanel : public QFrame
{
    Q_OBJECT

public:
    RetroPanel(const QString &title, const QColor &headerColor, QWidget *parent = nullptr);

    QVBoxLayout *body() const { return m_body; }

    // Small text on the right of the title strip
    void setHeaderInfo(const QString &text);

private:
    QLabel *m_info;
    QVBoxLayout *m_body;
};

#endif // RETROPANEL_H
