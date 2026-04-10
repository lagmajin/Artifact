module;
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QRect>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>
#include <wobjectdefs.h>

export module Artifact.Widgets.TimingEventView;

import std;
import Utils.Id;

export namespace Artifact {

struct TimingEventItem {
    QString id;
    QString label;
    int row = 0;
    int startFrame = 0;
    int endFrame = 0;
    bool selected = false;
};

class TimingEventView : public QWidget {
    W_OBJECT(TimingEventView)
private:
    class Impl;
    Impl* impl_;

    int frameToX(int frame) const;
    int xToFrame(int x) const;
    QRect eventRect(const TimingEventItem& item) const;
    TimingEventItem* eventAt(const QPoint& pos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

public:
    explicit TimingEventView(QWidget* parent = nullptr);
    ~TimingEventView();

    QVector<TimingEventItem> events() const;
    void setEvents(const QVector<TimingEventItem>& events);
    void clearEvents();

    int currentFrame() const;
    void setCurrentFrame(int frame);

    int visibleStartFrame() const;
    void setVisibleStartFrame(int frame);
    int visibleEndFrame() const;
    void setVisibleEndFrame(int frame);

    double pixelsPerFrame() const;
    void setPixelsPerFrame(double value);

    QString selectedEventId() const;
    void setSelectedEventId(const QString& id);

    void eventSelected(const QString& eventId) W_SIGNAL(eventSelected, eventId);
    void currentFrameChanged(int frame) W_SIGNAL(currentFrameChanged, frame);
    void eventsChanged() W_SIGNAL(eventsChanged);
};

} // namespace Artifact
