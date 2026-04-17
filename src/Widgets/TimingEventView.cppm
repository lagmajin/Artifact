module;

#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QSizePolicy>
#include <QWheelEvent>
#include <wobjectimpl.h>

module Artifact.Widgets.TimingEventView;

import std;
import Widgets.Utils.CSS;

namespace Artifact {

W_OBJECT_IMPL(TimingEventView)

namespace {
constexpr int kHeaderHeight = 30;
constexpr int kRowHeight = 34;
constexpr int kRowGap = 4;
constexpr int kLeftGutter = 12;
constexpr int kRightGutter = 12;
constexpr int kEdgeHitPx = 6;
constexpr double kMinPixelsPerFrame = 0.5;
constexpr double kMaxPixelsPerFrame = 96.0;
constexpr int kMinVisibleSpan = 8;

struct TimingTheme {
    QColor background;
    QColor surface;
    QColor border;
    QColor accent;
    QColor text;
};

TimingTheme timingTheme()
{
    const auto& theme = ArtifactCore::currentDCCTheme();
    return {
        QColor(theme.backgroundColor),
        QColor(theme.secondaryBackgroundColor),
        QColor(theme.borderColor),
        QColor(theme.accentColor),
        QColor(theme.textColor),
    };
}

QColor rowTint(const int row, const bool selected)
{
    const int hue = (row * 47) % 360;
    QColor base = QColor::fromHsv(hue, selected ? 78 : 45, selected ? 220 : 185);
    if (!base.isValid()) {
        base = QColor(118, 150, 220);
    }
    return base;
}

enum class DragMode {
    None,
    Move,
    ResizeStart,
    ResizeEnd,
};

} // namespace

class TimingEventView::Impl {
public:
    QVector<TimingEventItem> events_;
    int currentFrame_ = 0;
    int visibleStartFrame_ = 0;
    int visibleEndFrame_ = 180;
    double pixelsPerFrame_ = 6.0;
    QString selectedEventId_;
    bool dragging_ = false;
    DragMode dragMode_ = DragMode::None;
    QString dragEventId_;
    QPoint dragPressPos_;
    int dragStartFrame_ = 0;
    int dragEndFrame_ = 0;

    int rowCount() const
    {
        int rows = 1;
        for (const auto& item : events_) {
            rows = std::max(rows, item.row + 1);
        }
        return rows;
    }

    int contentLeft() const
    {
        return kLeftGutter;
    }

    int contentRight(const int width) const
    {
        return std::max(contentLeft(), width - kRightGutter);
    }

    int rowTop(const int row) const
    {
        return kHeaderHeight + row * (kRowHeight + kRowGap);
    }

    int frameToX(const int frame) const
    {
        return contentLeft() +
               static_cast<int>(std::lround((frame - visibleStartFrame_) * pixelsPerFrame_));
    }

    int xToFrame(const int x) const
    {
        return visibleStartFrame_ +
               static_cast<int>(std::lround((x - contentLeft()) / std::max(0.001, pixelsPerFrame_)));
    }

    QRect eventRect(const TimingEventItem& item, const int width) const
    {
        if (item.endFrame <= item.startFrame) {
            return {};
        }
        if (item.row < 0) {
            return {};
        }

        const int left = frameToX(item.startFrame);
        const int right = frameToX(item.endFrame);
        const int top = rowTop(item.row) + 4;
        const int height = kRowHeight - 8;
        const int minRight = std::max(left + 1, right);
        const int clampedLeft = std::clamp(left, contentLeft(), contentRight(width));
        const int clampedRight = std::clamp(minRight, contentLeft(), contentRight(width));
        return QRect(clampedLeft, top, std::max(1, clampedRight - clampedLeft), height);
    }

    TimingEventItem* eventAt(const QPoint& pos, const int width)
    {
        for (int i = events_.size() - 1; i >= 0; --i) {
            auto& item = events_[i];
            const QRect rect = eventRect(item, width);
            if (rect.contains(pos)) {
                return &item;
            }
        }
        return nullptr;
    }

    const TimingEventItem* eventAt(const QPoint& pos, const int width) const
    {
        return const_cast<Impl*>(this)->eventAt(pos, width);
    }

    void refreshSelectionFlags()
    {
        for (auto& item : events_) {
            item.selected = !selectedEventId_.isEmpty() && item.id == selectedEventId_;
        }
    }

    int clampVisibleSpan() const
    {
        return std::max(kMinVisibleSpan, visibleEndFrame_ - visibleStartFrame_);
    }

    void clampVisibleRange()
    {
        if (visibleEndFrame_ <= visibleStartFrame_) {
            visibleEndFrame_ = visibleStartFrame_ + kMinVisibleSpan;
        }
    }
};

TimingEventView::TimingEventView(QWidget* parent)
    : QWidget(parent), impl_(new Impl())
{
    setMouseTracking(true);
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(kHeaderHeight + kRowHeight + 24);
}

TimingEventView::~TimingEventView()
{
    delete impl_;
}

int TimingEventView::frameToX(const int frame) const
{
    return impl_ ? impl_->frameToX(frame) : 0;
}

int TimingEventView::xToFrame(const int x) const
{
    return impl_ ? impl_->xToFrame(x) : 0;
}

QRect TimingEventView::eventRect(const TimingEventItem& item) const
{
    return impl_ ? impl_->eventRect(item, width()) : QRect{};
}

TimingEventItem* TimingEventView::eventAt(const QPoint& pos)
{
    return impl_ ? impl_->eventAt(pos, width()) : nullptr;
}

QVector<TimingEventItem> TimingEventView::events() const
{
    return impl_ ? impl_->events_ : QVector<TimingEventItem>{};
}

void TimingEventView::setEvents(const QVector<TimingEventItem>& events)
{
    if (!impl_) {
        return;
    }

    impl_->events_ = events;
    bool selectionStillExists = false;
    for (const auto& item : impl_->events_) {
        if (item.id == impl_->selectedEventId_) {
            selectionStillExists = true;
            break;
        }
    }
    if (!selectionStillExists) {
        impl_->selectedEventId_.clear();
    }
    impl_->refreshSelectionFlags();
    updateGeometry();
    update();
    eventsChanged();
}

void TimingEventView::clearEvents()
{
    if (!impl_) {
        return;
    }

    impl_->events_.clear();
    impl_->selectedEventId_.clear();
    updateGeometry();
    update();
    eventsChanged();
}

int TimingEventView::currentFrame() const
{
    return impl_ ? impl_->currentFrame_ : 0;
}

void TimingEventView::setCurrentFrame(const int frame)
{
    if (!impl_) {
        return;
    }

    const int clamped = std::max(0, frame);
    if (impl_->currentFrame_ != clamped) {
        impl_->currentFrame_ = clamped;
        update();
        currentFrameChanged(clamped);
    }
}

int TimingEventView::visibleStartFrame() const
{
    return impl_ ? impl_->visibleStartFrame_ : 0;
}

void TimingEventView::setVisibleStartFrame(const int frame)
{
    if (!impl_) {
        return;
    }

    const int clamped = std::max(0, frame);
    if (impl_->visibleStartFrame_ != clamped) {
        impl_->visibleStartFrame_ = clamped;
        impl_->clampVisibleRange();
        update();
    }
}

int TimingEventView::visibleEndFrame() const
{
    return impl_ ? impl_->visibleEndFrame_ : 0;
}

void TimingEventView::setVisibleEndFrame(const int frame)
{
    if (!impl_) {
        return;
    }

    const int clamped = std::max(impl_->visibleStartFrame_ + 1, frame);
    if (impl_->visibleEndFrame_ != clamped) {
        impl_->visibleEndFrame_ = clamped;
        impl_->clampVisibleRange();
        update();
    }
}

double TimingEventView::pixelsPerFrame() const
{
    return impl_ ? impl_->pixelsPerFrame_ : 1.0;
}

void TimingEventView::setPixelsPerFrame(const double value)
{
    if (!impl_) {
        return;
    }

    const double clamped = std::clamp(value, kMinPixelsPerFrame, kMaxPixelsPerFrame);
    if (std::abs(impl_->pixelsPerFrame_ - clamped) > 0.0001) {
        impl_->pixelsPerFrame_ = clamped;
        updateGeometry();
        update();
    }
}

QString TimingEventView::selectedEventId() const
{
    return impl_ ? impl_->selectedEventId_ : QString{};
}

void TimingEventView::setSelectedEventId(const QString& id)
{
    if (!impl_) {
        return;
    }

    if (impl_->selectedEventId_ == id) {
        return;
    }

    impl_->selectedEventId_ = id;
    impl_->refreshSelectionFlags();
    update();
    eventSelected(id);
}

void TimingEventView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (!impl_) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const TimingTheme theme = timingTheme();
    const QRect outer = rect();
    painter.fillRect(outer, theme.background);

    const QColor topGlow = theme.surface.lighter(112);
    const QColor bottomGlow = theme.background.darker(116);
    QLinearGradient bgGrad(outer.topLeft(), outer.bottomLeft());
    bgGrad.setColorAt(0.0, topGlow);
    bgGrad.setColorAt(1.0, bottomGlow);
    painter.fillRect(outer, bgGrad);

    painter.setPen(theme.border);
    painter.drawRect(outer.adjusted(0, 0, -1, -1));

    const int rows = impl_->rowCount();
    const int rowBandWidth = std::max(0, width() - kLeftGutter - kRightGutter);
    for (int row = 0; row < rows; ++row) {
        const QRect band(kLeftGutter, impl_->rowTop(row), rowBandWidth, kRowHeight);
        painter.fillRect(band, (row % 2 == 0) ? theme.background.darker(108) : theme.background.darker(114));
        painter.setPen(theme.border.darker(120));
        painter.drawLine(band.left(), band.bottom(), band.right(), band.bottom());
    }

    QFont rulerFont = font();
    rulerFont.setPixelSize(9);
    painter.setFont(rulerFont);
    const QFontMetrics fm(rulerFont);

    const int visibleStart = impl_->visibleStartFrame_;
    const int visibleEnd = std::max(visibleStart + kMinVisibleSpan, impl_->visibleEndFrame_);
    const int visibleFrames = std::max(1, visibleEnd - visibleStart);
    const int frameSpan = std::max(1, visibleFrames);
    const double ppf = std::max(0.001, impl_->pixelsPerFrame_);
    const int firstMajor = std::max(visibleStart, (visibleStart / 10) * 10);
    const int majorStep = std::max(1, frameSpan / 8);
    const int minorStep = std::max(1, majorStep / 4);
    double lastLabelRight = -1.0;
    for (int frame = visibleStart; frame <= visibleEnd + minorStep; frame += minorStep) {
        const double x = kLeftGutter + (frame - visibleStart) * ppf;
        if (x < kLeftGutter - 1 || x > width() - kRightGutter + 1) {
            continue;
        }
        const bool major = ((frame - firstMajor) % majorStep) == 0;
        const int tickTop = major ? 2 : 9;
        const int tickBottom = kHeaderHeight - 3;
        painter.setPen(QPen(major ? theme.border.lighter(145) : theme.border.darker(118), 1));
        painter.drawLine(QPointF(x, tickTop), QPointF(x, tickBottom));
        if (major) {
            const QString label = QString::number(frame);
            const double labelWidth = fm.horizontalAdvance(label);
            const double labelX = x + 4.0;
            if (labelX > lastLabelRight + 5.0) {
                painter.setPen(theme.text.lighter(175));
                painter.drawText(QRectF(labelX, 0.0, labelWidth + 8.0, kHeaderHeight - 2),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 label);
                lastLabelRight = labelX + labelWidth;
            }
        }
    }

    painter.setPen(theme.accent.lighter(180));
    painter.drawLine(QPointF(kLeftGutter, kHeaderHeight - 1), QPointF(width() - kRightGutter, kHeaderHeight - 1));

    for (const auto& item : impl_->events_) {
        const QRect rect = impl_->eventRect(item, width());
        if (!rect.isValid()) {
            continue;
        }

        const bool selected = !impl_->selectedEventId_.isEmpty() && item.id == impl_->selectedEventId_;
        const QColor fill = rowTint(item.row, selected);
        QColor body = fill;
        body.setAlpha(selected ? 235 : 200);
        QColor border = selected ? theme.accent.lighter(135) : fill.lighter(140);

        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        grad.setColorAt(0.0, body.lighter(selected ? 120 : 112));
        grad.setColorAt(1.0, body.darker(selected ? 132 : 120));

        painter.setPen(QPen(border, selected ? 2 : 1));
        painter.setBrush(grad);
        painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 6, 6);

        painter.setPen(theme.background.darker(155));
        painter.drawRoundedRect(rect.adjusted(1, 1, -2, -2), 5, 5);

        QFont labelFont = font();
        labelFont.setBold(true);
        painter.setFont(labelFont);
        const QRect textRect = rect.adjusted(8, 0, -8, 0);
        const QString text = fm.elidedText(item.label.isEmpty() ? item.id : item.label,
                                           Qt::ElideRight,
                                           std::max(0, textRect.width()));
        painter.setPen(theme.text.lighter(selected ? 190 : 165));
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    }

    const int clampedCurrentFrame = std::max(0, impl_->currentFrame_);
    const double playheadX = kLeftGutter + (clampedCurrentFrame - visibleStart) * ppf;
    if (playheadX >= kLeftGutter - 2.0 && playheadX <= width() - kRightGutter + 2.0) {
        const QColor playheadColor(255, 106, 71);
        const qreal headTop = 2.0;
        const qreal headHeight = 10.0;
        const qreal headWidth = 14.0;
        const qreal stemTop = headTop + headHeight + 1.0;

        QPainterPath marker;
        marker.moveTo(playheadX, headTop + headHeight);
        marker.lineTo(playheadX - headWidth * 0.5, headTop);
        marker.lineTo(playheadX + headWidth * 0.5, headTop);
        marker.closeSubpath();

        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(QColor(18, 18, 18, 140), 1));
        painter.setBrush(playheadColor);
        painter.drawPath(marker);

        painter.setPen(QPen(playheadColor, 2));
        painter.drawLine(QPointF(playheadX, stemTop), QPointF(playheadX, height()));
    }
}

void TimingEventView::mousePressEvent(QMouseEvent* event)
{
    if (!impl_ || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const TimingEventItem* hit = impl_->eventAt(event->pos(), width());
    if (!hit) {
        setSelectedEventId(QString{});
        setCurrentFrame(impl_->xToFrame(event->pos().x()));
        update();
        return;
    }

    setSelectedEventId(hit->id);
    impl_->dragging_ = true;
    impl_->dragEventId_ = hit->id;
    impl_->dragPressPos_ = event->pos();
    impl_->dragStartFrame_ = hit->startFrame;
    impl_->dragEndFrame_ = hit->endFrame;

    const QRect rect = impl_->eventRect(*hit, width());
    const bool nearLeftEdge = std::abs(event->pos().x() - rect.left()) <= kEdgeHitPx;
    const bool nearRightEdge = std::abs(event->pos().x() - rect.right()) <= kEdgeHitPx;
    impl_->dragMode_ = nearLeftEdge ? DragMode::ResizeStart
                                    : (nearRightEdge ? DragMode::ResizeEnd : DragMode::Move);
    setCursor(impl_->dragMode_ == DragMode::Move ? Qt::ClosedHandCursor : Qt::SizeHorCursor);
}

void TimingEventView::mouseMoveEvent(QMouseEvent* event)
{
    if (!impl_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (impl_->dragging_) {
        auto* item = impl_->eventAt(impl_->dragPressPos_, width());
        if (!item || item->id != impl_->dragEventId_) {
            item = nullptr;
            for (auto& candidate : impl_->events_) {
                if (candidate.id == impl_->dragEventId_) {
                    item = &candidate;
                    break;
                }
            }
        }
        if (!item) {
            return;
        }

        const int deltaFrames =
            static_cast<int>(std::lround((event->pos().x() - impl_->dragPressPos_.x()) /
                                         std::max(0.001, impl_->pixelsPerFrame_)));
        switch (impl_->dragMode_) {
        case DragMode::Move: {
            const int duration = std::max(1, impl_->dragEndFrame_ - impl_->dragStartFrame_);
            const int start = std::max(0, impl_->dragStartFrame_ + deltaFrames);
            item->startFrame = start;
            item->endFrame = start + duration;
            break;
        }
        case DragMode::ResizeStart: {
            const int newStart = std::clamp(impl_->dragStartFrame_ + deltaFrames, 0, impl_->dragEndFrame_ - 1);
            item->startFrame = newStart;
            break;
        }
        case DragMode::ResizeEnd: {
            const int newEnd = std::max(impl_->dragStartFrame_ + 1, impl_->dragEndFrame_ + deltaFrames);
            item->endFrame = newEnd;
            break;
        }
        case DragMode::None:
            break;
        }

        impl_->refreshSelectionFlags();
        update();
        eventsChanged();
        return;
    }

    const TimingEventItem* hit = impl_->eventAt(event->pos(), width());
    if (hit) {
        const QRect rect = impl_->eventRect(*hit, width());
        if (std::abs(event->pos().x() - rect.left()) <= kEdgeHitPx ||
            std::abs(event->pos().x() - rect.right()) <= kEdgeHitPx) {
            setCursor(Qt::SizeHorCursor);
        } else {
            setCursor(Qt::OpenHandCursor);
        }
    } else {
        unsetCursor();
    }
}

void TimingEventView::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    if (!impl_) {
        return;
    }

    if (impl_->dragging_) {
        impl_->dragging_ = false;
        impl_->dragMode_ = DragMode::None;
        impl_->dragEventId_.clear();
        unsetCursor();
        update();
    }
}

void TimingEventView::wheelEvent(QWheelEvent* event)
{
    if (!impl_) {
        QWidget::wheelEvent(event);
        return;
    }

    const QPoint delta = event->angleDelta().isNull() ? event->pixelDelta() : event->angleDelta();
    if (delta.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }

    if (event->modifiers() & Qt::ControlModifier) {
        const double scale = delta.y() > 0 ? 0.88 : 1.14;
        const double oldPixelsPerFrame = std::max(0.001, impl_->pixelsPerFrame_);
        const int anchorFrame = impl_->xToFrame(event->position().x());
        const double anchorOffset = event->position().x() - kLeftGutter;
        const double newPixelsPerFrame = std::clamp(oldPixelsPerFrame * scale, kMinPixelsPerFrame, kMaxPixelsPerFrame);
        impl_->pixelsPerFrame_ = newPixelsPerFrame;

        const int newVisibleStart =
            anchorFrame - static_cast<int>(std::lround(anchorOffset / std::max(0.001, newPixelsPerFrame)));
        impl_->visibleStartFrame_ = std::max(0, newVisibleStart);
        impl_->clampVisibleRange();
        update();
        event->accept();
        return;
    }

    const int frameDelta = (delta.y() / 120) * std::max(1, static_cast<int>(std::lround(6.0 / std::max(0.001, impl_->pixelsPerFrame_))));
    impl_->visibleStartFrame_ = std::max(0, impl_->visibleStartFrame_ - frameDelta);
    impl_->visibleEndFrame_ = std::max(impl_->visibleStartFrame_ + kMinVisibleSpan,
                                       impl_->visibleEndFrame_ - frameDelta);
    update();
    event->accept();
}

} // namespace Artifact
