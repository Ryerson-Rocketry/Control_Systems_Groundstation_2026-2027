#ifndef LINEPLOT_H
#define LINEPLOT_H

/*
 * lineplot.h
 * Purpose:
 *   A live line plot against mission time (req 9), drawn by hand with
 *   QPainter as a duck-hunting field: blue sky, grass along the bottom,
 *   a bird flying at the head of each line while the rocket is in the
 *   air, and a dog popping up with a sign wherever the phase changed.
 *   Shows the last 3 minutes; the y range grows if a value goes past it.
 *   A NaN value (not in the packet) leaves a gap and a note on the plot.
 *
 *   Usage: addSeries() once per line, then append() each new sample.
 *   Call addEvent() on a phase change and setFlying() as it lifts off
 *   and lands.
 */

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

class LinePlot : public QWidget
{
    Q_OBJECT

public:
    LinePlot(const QString &title, double yMin, double yMax, QWidget *parent = nullptr);

    // Returns the series index to pass to append()
    int addSeries(const QString &name, const QColor &color, Qt::PenStyle style = Qt::SolidLine);
    void append(int series, double time, double value);
    // Dog with a sign reading label at this mission time
    void addEvent(double time, const QString &label);
    // Birds on the line heads only while flying
    void setFlying(bool flying);
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Series
    {
        QString name;
        QColor color;
        Qt::PenStyle style;
        QVector<QPointF> points;  // (mission time s, value)
    };

    struct Event
    {
        double time;
        QString label;
    };

    QString m_title;
    double m_yMin;
    double m_yMax;
    double m_defaultYMin;
    double m_defaultYMax;
    double m_latestTime = 0;
    QVector<Series> m_series;
    QVector<Event> m_events;
    bool m_flying = false;
    bool m_noData = false;  // the latest packet did not carry this plot's value
};

#endif // LINEPLOT_H
