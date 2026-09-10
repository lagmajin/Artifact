module;
#include <algorithm>
#include <cmath>
#include <QColor>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QVector>
#include <wobjectimpl.h>

module Artifact.Widgets.AnimationTimelineWidget;

import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layers.Selection.Manager;
import Artifact.Service.Project;
import Event.Bus;
import Undo.UndoManager;
import Widgets.Utils.CSS;
import std;

namespace Artifact {

namespace {
struct SemanticSpan { QString name; qint64 begin = 0; qint64 end = 0; QColor color; };

QVector<SemanticSpan> spansForLayer(const ArtifactAbstractLayerPtr& layer) {
  QVector<SemanticSpan> spans;
  if (!layer) return spans;
  const qint64 in = layer->inPoint().framePosition();
  const qint64 out = std::max(in + 1, layer->outPoint().framePosition());
  const auto* composition = static_cast<const ArtifactAbstractComposition*>(layer->composition());
  const int64_t frameScale = composition
      ? std::max<int64_t>(1, static_cast<int64_t>(std::llround(
            composition->frameRate().framerate())))
      : 1;
  QVector<qint64> times;
  for (const auto& group : layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
      if (!property || !property->isAnimatable()) continue;
      for (const auto& key : property->getKeyFrames()) {
        times.push_back(std::clamp<qint64>(key.time.rescaledTo(frameScale), in, out));
      }
    }
  }
  std::sort(times.begin(), times.end());
  times.erase(std::unique(times.begin(), times.end()), times.end());
  if (times.size() < 2) {
    spans.push_back({QStringLiteral("IDLE"), in, out, QColor(91, 99, 110)});
    return spans;
  }
  if (in < times.front()) {
    spans.push_back({QStringLiteral("IDLE"), in, times.front(), QColor(91, 99, 110)});
  }
  spans.push_back({QStringLiteral("ENTER"), times[0], times[1], QColor(74, 160, 220)});
  const qint64 idleThreshold = std::max<qint64>(2, frameScale / 2);
  for (int i = 1; i + 2 < times.size(); ++i) {
    const qint64 duration = times[i + 1] - times[i];
    spans.push_back({duration > idleThreshold ? QStringLiteral("IDLE")
                                             : QStringLiteral("ANIMATE"),
                     times[i], times[i + 1],
                     duration > idleThreshold ? QColor(91, 99, 110)
                                              : QColor(177, 118, 230)});
  }
  if (times.size() > 2) {
    spans.push_back({QStringLiteral("EXIT"), times[times.size() - 2], times.back(),
                     QColor(226, 122, 109)});
  }
  if (times.back() < out) {
    spans.push_back({QStringLiteral("IDLE"), times.back(), out, QColor(91, 99, 110)});
  }
  return spans;
}
}

class ArtifactAnimationTimelineWidget::Impl {
public:
  ArtifactCore::CompositionID compositionId;
  QString layerName;
  QVector<SemanticSpan> spans;
  ArtifactAbstractLayerPtr layer;
  int dragSpan = -1;
  qint64 dragOriginalBegin = 0;
  qint64 dragOriginalEnd = 0;
  ArtifactCore::EventBus::Subscription changedSubscription;
  ArtifactCore::EventBus::Subscription selectionSubscription;
};

W_OBJECT_IMPL(ArtifactAnimationTimelineWidget)

ArtifactAnimationTimelineWidget::ArtifactAnimationTimelineWidget(QWidget* parent)
    : QWidget(parent), impl_(new Impl()) {
  setMinimumHeight(96);
  setFocusPolicy(Qt::StrongFocus);
  impl_->changedSubscription = ArtifactCore::globalEventBus().subscribe<CompositionChangedEvent>(
      [this](const CompositionChangedEvent& event) {
        if (impl_ && event.compositionId == impl_->compositionId.toString()) refresh();
      });
  impl_->selectionSubscription = ArtifactCore::globalEventBus().subscribe<LayerSelectionChangedEvent>(
      [this](const LayerSelectionChangedEvent&) { refresh(); });
}

ArtifactAnimationTimelineWidget::~ArtifactAnimationTimelineWidget() { delete impl_; }

void ArtifactAnimationTimelineWidget::setComposition(const ArtifactCore::CompositionID& compositionId) {
  if (!impl_ || impl_->compositionId == compositionId) return;
  impl_->compositionId = compositionId;
  refresh();
}

void ArtifactAnimationTimelineWidget::refresh() {
  if (!impl_) return;
  impl_->spans.clear();
  impl_->layerName.clear();
  impl_->layer.reset();
  auto* service = ArtifactProjectService::instance();
  const auto result = service ? service->findComposition(impl_->compositionId) : FindCompositionResult{};
  const auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
  if (composition) {
    auto* selection = ArtifactLayerSelectionManager::instance();
    const auto layer = selection ? selection->currentLayer() : ArtifactAbstractLayerPtr{};
    if (layer && layer->composition() == composition.get()) {
      impl_->layerName = layer->layerName();
      impl_->layer = layer;
      impl_->spans = spansForLayer(layer);
    }
  }
  update();
}

void ArtifactAnimationTimelineWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  const auto& theme = ArtifactCore::currentDCCTheme();
  painter.fillRect(rect(), QColor(theme.secondaryBackgroundColor));
  const QRect content = rect().adjusted(10, 8, -10, -8);
  painter.setPen(QColor(theme.textColor));
  painter.drawText(QRect(content.left(), content.top(), content.width(), 18),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   impl_ && !impl_->layerName.isEmpty()
                       ? QStringLiteral("Animation Timeline · %1").arg(impl_->layerName)
                       : QStringLiteral("Animation Timeline · select a layer"));
  if (!impl_ || impl_->spans.isEmpty()) return;
  const qint64 start = impl_->spans.front().begin;
  const qint64 end = std::max<qint64>(start + 1, impl_->spans.back().end);
  const QRect bar(content.left(), content.top() + 28, content.width(), std::max(24, content.height() - 36));
  for (const auto& span : impl_->spans) {
    const double begin = static_cast<double>(span.begin - start) / (end - start);
    const double finish = static_cast<double>(span.end - start) / (end - start);
    QRect segment(bar.left() + static_cast<int>(begin * bar.width()), bar.top(),
                  std::max(1, static_cast<int>((finish - begin) * bar.width())), bar.height());
    painter.fillRect(segment.adjusted(1, 1, -1, -1), span.color);
    painter.setPen(Qt::white);
    painter.drawText(segment.adjusted(5, 1, -5, -1), Qt::AlignCenter,
                     QStringLiteral("%1  %2f").arg(span.name).arg(span.end - span.begin));
  }
}

void ArtifactAnimationTimelineWidget::mousePressEvent(QMouseEvent* event) {
  if (!impl_ || !event || event->button() != Qt::LeftButton || impl_->spans.size() < 2) {
    QWidget::mousePressEvent(event);
    return;
  }
  const QRect bar = rect().adjusted(10, 36, -10, -8);
  if (!bar.contains(event->pos())) return;
  const qint64 start = impl_->spans.front().begin;
  const qint64 end = std::max<qint64>(start + 1, impl_->spans.back().end);
  for (int i = 0; i + 1 < impl_->spans.size(); ++i) {
    const double ratio = static_cast<double>(impl_->spans[i].end - start) / (end - start);
    const int boundaryX = bar.left() + static_cast<int>(ratio * bar.width());
    if (std::abs(event->pos().x() - boundaryX) <= 8) {
      impl_->dragSpan = i;
      impl_->dragOriginalBegin = impl_->spans[i].begin;
      impl_->dragOriginalEnd = impl_->spans[i].end;
      setCursor(Qt::SplitHCursor);
      event->accept();
      return;
    }
  }
}

void ArtifactAnimationTimelineWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!impl_ || !event || impl_->dragSpan < 0) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  const QRect bar = rect().adjusted(10, 36, -10, -8);
  const qint64 rangeBegin = impl_->spans.front().begin;
  const qint64 rangeEnd = std::max(rangeBegin + 1, impl_->spans.back().end);
  const double ratio = std::clamp(
      (event->position().x() - bar.left()) / std::max(1.0, static_cast<double>(bar.width())),
      0.0, 1.0);
  qint64 frame = rangeBegin + static_cast<qint64>(std::llround(ratio * (rangeEnd - rangeBegin)));
  const qint64 lower = impl_->spans[impl_->dragSpan].begin + 1;
  const qint64 upper = impl_->spans[impl_->dragSpan + 1].end - 1;
  frame = std::clamp(frame, lower, upper);
  impl_->spans[impl_->dragSpan].end = frame;
  impl_->spans[impl_->dragSpan + 1].begin = frame;
  update();
  event->accept();
}

bool ArtifactAnimationTimelineWidget::commitClipRetime() {
  if (!impl_ || !impl_->layer || impl_->dragSpan < 0) return false;
  const qint64 oldBegin = impl_->dragOriginalBegin;
  const qint64 oldEnd = impl_->dragOriginalEnd;
  const qint64 newEnd = impl_->spans[impl_->dragSpan].end;
  if (oldEnd <= oldBegin || newEnd == oldEnd) return false;
  const auto* composition = static_cast<const ArtifactAbstractComposition*>(impl_->layer->composition());
  const int64_t scale = composition
      ? std::max<int64_t>(1, static_cast<int64_t>(std::llround(composition->frameRate().framerate())))
      : 1;
  auto macro = std::make_unique<MacroUndoCommand>(
      QStringLiteral("Retime %1 clip").arg(impl_->spans[impl_->dragSpan].name));
  int changedProperties = 0;
  for (const auto& group : impl_->layer->getLayerPropertyGroups()) {
    for (const auto& property : group.sortedProperties()) {
      if (!property || !property->isAnimatable()) continue;
      const auto before = property->getKeyFrames();
      auto after = before;
      bool changed = false;
      for (auto& key : after) {
        const qint64 frame = key.time.rescaledTo(scale);
        if (frame < oldBegin || frame > oldEnd) continue;
        const double t = static_cast<double>(frame - oldBegin) / (oldEnd - oldBegin);
        const qint64 retimed = oldBegin + static_cast<qint64>(std::llround(t * (newEnd - oldBegin)));
        key.time = RationalTime(retimed, scale);
        changed = changed || retimed != frame;
      }
      if (!changed) continue;
      macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
          impl_->layer, property->getName(), before, after,
          QStringLiteral("Retime %1").arg(property->getName())));
      ++changedProperties;
    }
  }
  if (changedProperties == 0) return false;
  auto* manager = UndoManager::instance();
  const bool committed = manager && manager->push(std::move(macro));
  if (committed) {
    impl_->layer->changed();
    ArtifactCore::globalEventBus().publish<LayerChangedEvent>(LayerChangedEvent{
        composition ? composition->id().toString() : QString(), impl_->layer->id().toString(),
        LayerChangedEvent::ChangeType::Modified});
  }
  return committed;
}

void ArtifactAnimationTimelineWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!impl_ || !event || event->button() != Qt::LeftButton || impl_->spans.isEmpty()) return;
  if (impl_->dragSpan >= 0) {
    commitClipRetime();
    impl_->dragSpan = -1;
    unsetCursor();
    refresh();
    event->accept();
    return;
  }
  const QRect bar = rect().adjusted(10, 36, -10, -8);
  if (!bar.contains(event->pos())) return;
  const qint64 start = impl_->spans.front().begin;
  const qint64 end = std::max(start + 1, impl_->spans.back().end);
  const double ratio = std::clamp((event->position().x() - bar.left()) / std::max(1.0, static_cast<double>(bar.width())), 0.0, 1.0);
  ArtifactCore::globalEventBus().publish<TimelineSeekRequestedEvent>(
      TimelineSeekRequestedEvent{static_cast<double>(
          start + static_cast<qint64>(std::llround(ratio * (end - start))))});
}

} // namespace Artifact
