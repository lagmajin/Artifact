module;

#include <QColor>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

#include "Timeline/TimelinePlayheadDraw.hpp"

export module Artifact.Widgets.TimelinePresentation;

import Widgets.Utils.CSS;
import Artifact.Widget.WorkAreaControlWidget;
import Widget.CurveEditor;
import Artifact.Timeline.ScrubBar;
import Artifact.Timeline.NavigatorWidget;
import Artifact.Timeline.TrackPainterView;
import Artifact.Event.Types;
import Event.Bus;
import Frame.Position;

namespace Artifact {

namespace {

class TimelinePlayheadOverlayWidget final : public QWidget {
public:
  TimelinePlayheadOverlayWidget(ArtifactTimelineNavigatorWidget *navigator,
                                ArtifactTimelineScrubBar *scrubBar,
                                ArtifactTimelineTrackPainterView *trackView,
                                QWidget *parent)
      : QWidget(parent), scrubBar_(scrubBar), trackView_(trackView) {
    Q_UNUSED(navigator);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    if (trackView_) {
      trackView_->installEventFilter(this);
    }
  }

  void syncGeometryToPanel() {
    if (!enabled_) {
      hide();
      return;
    }

    auto *panel = parentWidget();
    if (!panel || !scrubBar_ || !trackView_) {
      hide();
      return;
    }

    QWidget *topAnchor = static_cast<QWidget *>(scrubBar_);
    const int top = topAnchor->mapTo(panel, QPoint(0, 0)).y();
    const int panelHeight = std::max(0, panel->height());
    const QRect nextGeometry(0, std::clamp(top, 0, panelHeight),
                             std::max(0, panel->width()),
                             std::max(0, panelHeight - top));
    if (geometry() != nextGeometry) {
      setGeometry(nextGeometry);
      lastX_ = -9999;
      update();
    }
    show();
    raise();
  }

  void updatePlayhead() {
    syncGeometryToPanel();
    if (!isVisible()) {
      return;
    }

    const int newX = currentPlayheadX();
    constexpr int kMargin = 16;
    if (lastX_ == -9999) {
      update();
    } else {
      const int left = std::min(lastX_, newX) - kMargin;
      const int right = std::max(lastX_, newX) + kMargin + 1;
      update(QRect(left, 0, right - left, height()));
    }
    lastX_ = newX;
  }

  void setOverlayEnabled(const bool enabled) {
    if (enabled_ == enabled) {
      return;
    }
    enabled_ = enabled;
    lastX_ = -9999;
    if (!enabled_) {
      hide();
      return;
    }
    syncGeometryToPanel();
    update();
  }

  static constexpr int kPlayheadHitRadius = 14;

protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched != trackView_ || !event) {
      return QWidget::eventFilter(watched, event);
    }

    auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);
    if (!mouseEvent) {
      return QWidget::eventFilter(watched, event);
    }

    const QPointF localPoint = mapFrom(trackView_, mouseEvent->position().toPoint());
    const int playheadX = currentPlayheadX();
    switch (event->type()) {
    case QEvent::MouseButtonPress:
      if (mouseEvent->button() == Qt::LeftButton &&
          std::abs(localPoint.x() - static_cast<double>(playheadX)) <=
              kPlayheadHitRadius) {
        dragging_ = true;
        trackView_->setCursor(Qt::SizeHorCursor);
        return true;
      }
      break;
    case QEvent::MouseMove:
      if (dragging_ && (mouseEvent->buttons() & Qt::LeftButton)) {
        const double ppf = std::max(0.001, trackView_->pixelsPerFrame());
        const double frame = std::clamp(
            (mouseEvent->position().x() + trackView_->horizontalOffset()) / ppf,
            0.0, std::max(0.0, trackView_->durationFrames() - 1.0));
        trackView_->setCurrentFrame(frame);
        ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(
            TimelineSeekRequestedEvent{frame});
        updatePlayhead();
        return true;
      }
      break;
    case QEvent::MouseButtonRelease:
      if (dragging_ && mouseEvent->button() == Qt::LeftButton) {
        dragging_ = false;
        trackView_->unsetCursor();
        return true;
      }
      break;
    default:
      break;
    }
    return QWidget::eventFilter(watched, event);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (!event || event->button() != Qt::LeftButton || !trackView_) {
      event ? event->ignore() : void();
      return;
    }

    const int playheadX = currentPlayheadX();
    if (std::abs(event->position().x() - static_cast<double>(playheadX)) >
        kPlayheadHitRadius) {
      event->ignore();
      return;
    }

    dragging_ = true;
    setCursor(Qt::SizeHorCursor);
    event->accept();
  }

  void mouseMoveEvent(QMouseEvent *event) override {
    if (!event || !dragging_ || !trackView_ || !scrubBar_) {
      event ? event->ignore() : void();
      return;
    }

    const QPoint scrubPoint = mapTo(scrubBar_, event->position().toPoint());
    const double frame = frameAtScrubX(scrubPoint.x());
    trackView_->setCurrentFrame(frame);
    scrubBar_->setCurrentFrame(FramePosition(static_cast<int>(std::llround(frame))));
    scrubBar_->setVisualFrame(frame);
    ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(
        TimelineSeekRequestedEvent{frame});
    updatePlayhead();
    event->accept();
  }

  void mouseReleaseEvent(QMouseEvent *event) override {
    if (!event || event->button() != Qt::LeftButton || !dragging_) {
      event ? event->ignore() : void();
      return;
    }
    dragging_ = false;
    unsetCursor();
    event->accept();
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);
    if (!trackView_ || width() <= 0 || height() <= 0) {
      return;
    }

    const int x = currentPlayheadX();
    if (x < -16 || x > width() + 16) {
      return;
    }

    QPainter painter(this);
    TimelinePlayheadDraw::drawPlayhead(
        painter, static_cast<qreal>(x), 0.0,
        static_cast<qreal>(height()) - 1.0, true, 0.0, 12.0, 14.0);
  }

private:
  double frameAtScrubX(const int x) const {
    const double lastFrame = std::max(0.0, trackView_->durationFrames() - 1.0);
    if (lastFrame <= 0.0) {
      return 0.0;
    }

    double low = 0.0;
    double high = lastFrame;
    for (int i = 0; i < 32; ++i) {
      const double mid = (low + high) * 0.5;
      if (scrubBar_->rulerFrameToX(mid) < x) {
        low = mid;
      } else {
        high = mid;
      }
    }
    return std::clamp((low + high) * 0.5, 0.0, lastFrame);
  }

  int currentPlayheadX() const {
    if (!scrubBar_ || !parentWidget()) {
      return 0;
    }

    const double frame = std::max(0.0, trackView_ ? trackView_->currentFrame() : 0.0);
    const QPoint panelPoint = scrubBar_->mapTo(
        parentWidget(), QPoint(scrubBar_->rulerFrameToX(frame), 0));
    return panelPoint.x() - x();
  }

  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  ArtifactTimelineTrackPainterView *trackView_ = nullptr;
  bool enabled_ = true;
  bool dragging_ = false;
  int lastX_ = -9999;
};

class TimelineRightPanelWidget final : public QWidget {
public:
  TimelineRightPanelWidget(ArtifactTimelineNavigatorWidget *navigator,
                           ArtifactTimelineScrubBar *scrubBar,
                           WorkAreaControl *workArea,
                           ArtifactTimelineTrackPainterView *painterTrackView,
                           QWidget *gpuTimelineContainer,
                           QWidget *curveHeader,
                           ArtifactCurveEditorWidget *curveEditor,
                           QWidget *parent = nullptr)
      : QWidget(parent), navigator_(navigator), scrubBar_(scrubBar),
        workArea_(workArea), painterTrackView_(painterTrackView) {
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAttribute(Qt::WA_NoSystemBackground, true);

    auto *rightPanelLayout = new QVBoxLayout(this);
    rightPanelLayout->setSpacing(0);
    rightPanelLayout->setContentsMargins(0, 0, 0, 0);

    timelinePainterPage_ = new QWidget(this);
    timelinePainterPage_->setObjectName(QStringLiteral("timelinePainterPage"));
    auto *timelinePainterLayout = new QVBoxLayout(timelinePainterPage_);
    timelinePainterLayout->setContentsMargins(0, 0, 0, 0);
    timelinePainterLayout->setSpacing(0);
    if (painterTrackView_) {
      timelinePainterLayout->addWidget(painterTrackView_, 1);
    }

    timelineGpuPage_ = new QWidget(this);
    timelineGpuPage_->setObjectName(QStringLiteral("timelineGpuPreviewPage"));
    auto *timelineGpuLayout = new QVBoxLayout(timelineGpuPage_);
    timelineGpuLayout->setContentsMargins(0, 0, 0, 0);
    timelineGpuLayout->setSpacing(0);
    if (gpuTimelineContainer) {
      timelineGpuLayout->addWidget(gpuTimelineContainer, 1);
    }

    curveEditorPage_ = new QWidget(this);
    curveEditorPage_->setObjectName(QStringLiteral("timelineCurveEditorPage"));
    auto *curvePanelLayout = new QVBoxLayout(curveEditorPage_);
    curvePanelLayout->setContentsMargins(0, 0, 0, 0);
    curvePanelLayout->setSpacing(0);
    if (curveHeader) {
      curvePanelLayout->addWidget(curveHeader);
    }
    if (curveEditor) {
      curvePanelLayout->addWidget(curveEditor, 1);
    }

    timelineModeStack_ = new QStackedWidget(this);
    timelineModeStack_->addWidget(timelinePainterPage_);
    timelineModeStack_->addWidget(timelineGpuPage_);
    timelineModeStack_->addWidget(curveEditorPage_);
    timelineModeStack_->setCurrentWidget(timelinePainterPage_);

    if (navigator_) {
      rightPanelLayout->addWidget(navigator_);
    }
    if (scrubBar_) {
      rightPanelLayout->addWidget(scrubBar_);
    }
    if (workArea_) {
      rightPanelLayout->addWidget(workArea_);
    }
    rightPanelLayout->addWidget(timelineModeStack_, 1);

    playheadOverlay_ =
        new TimelinePlayheadOverlayWidget(navigator_, scrubBar_,
                                          painterTrackView_, this);
    playheadOverlay_->syncGeometryToPanel();
  }

  QWidget *timelinePainterPage() const { return timelinePainterPage_; }
  QWidget *timelineGpuPage() const { return timelineGpuPage_; }
  QWidget *curveEditorPage() const { return curveEditorPage_; }
  QStackedWidget *timelineModeStack() const { return timelineModeStack_; }
  void syncPlayheadOverlay() {
    if (playheadOverlay_) {
      playheadOverlay_->updatePlayhead();
    }
  }

  void setPlayheadOverlayEnabled(const bool enabled) {
    if (playheadOverlay_) {
      playheadOverlay_->setOverlayEnabled(enabled);
    }
  }

protected:
  void resizeEvent(QResizeEvent *event) override {
    QWidget::resizeEvent(event);
    if (playheadOverlay_) {
      playheadOverlay_->syncGeometryToPanel();
    }
  }

  void showEvent(QShowEvent *event) override {
    QWidget::showEvent(event);
    if (playheadOverlay_) {
      playheadOverlay_->syncGeometryToPanel();
    }
  }

  void paintEvent(QPaintEvent *event) override {
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QRect bounds = rect();
    const auto &theme = ArtifactCore::currentDCCTheme();
    const QColor base(theme.backgroundColor);
    const QColor border(theme.borderColor);
    const QColor topShade(theme.secondaryBackgroundColor);

    painter.fillRect(bounds, base);
    painter.setPen(QPen(border, 1));
    painter.drawRect(bounds.adjusted(0, 0, -1, -1));

    QColor accent = topShade;
    accent.setAlpha(28);
    painter.fillRect(QRect(bounds.left(), bounds.top(), bounds.width(), 1),
                     accent);
  }

private:
  ArtifactTimelineNavigatorWidget *navigator_ = nullptr;
  ArtifactTimelineScrubBar *scrubBar_ = nullptr;
  WorkAreaControl *workArea_ = nullptr;
  ArtifactTimelineTrackPainterView *painterTrackView_ = nullptr;
  QWidget *timelinePainterPage_ = nullptr;
  QWidget *timelineGpuPage_ = nullptr;
  QWidget *curveEditorPage_ = nullptr;
  QStackedWidget *timelineModeStack_ = nullptr;
  TimelinePlayheadOverlayWidget *playheadOverlay_ = nullptr;
};

} // namespace

export QWidget *createTimelineRightPanel(
    ArtifactTimelineNavigatorWidget *navigator,
    ArtifactTimelineScrubBar *scrubBar, WorkAreaControl *workArea,
    ArtifactTimelineTrackPainterView *painterTrackView,
    QWidget *gpuTimelineContainer, QWidget *curveHeader,
    ArtifactCurveEditorWidget *curveEditor, QWidget *parent) {
  return new TimelineRightPanelWidget(navigator, scrubBar, workArea,
                                      painterTrackView, gpuTimelineContainer,
                                      curveHeader, curveEditor, parent);
}

export QWidget *timelineRightPanelPainterPage(QWidget *panel) {
  auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel);
  return rightPanel ? rightPanel->timelinePainterPage() : nullptr;
}

export QWidget *timelineRightPanelGpuPage(QWidget *panel) {
  auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel);
  return rightPanel ? rightPanel->timelineGpuPage() : nullptr;
}

export QWidget *timelineRightPanelCurveEditorPage(QWidget *panel) {
  auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel);
  return rightPanel ? rightPanel->curveEditorPage() : nullptr;
}

export QStackedWidget *timelineRightPanelModeStack(QWidget *panel) {
  auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel);
  return rightPanel ? rightPanel->timelineModeStack() : nullptr;
}

export void setTimelineRightPanelPlayheadOverlayEnabled(QWidget *panel,
                                                         const bool enabled) {
  if (auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel)) {
    rightPanel->setPlayheadOverlayEnabled(enabled);
  }
}

export void syncTimelineRightPanelPlayheadOverlay(QWidget *panel) {
  if (auto *rightPanel = dynamic_cast<TimelineRightPanelWidget *>(panel)) {
    rightPanel->syncPlayheadOverlay();
  }
}

} // namespace Artifact
