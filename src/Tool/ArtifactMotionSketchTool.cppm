module;
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <QPointF>
#include <QElapsedTimer>
#include <wobjectimpl.h>

module Artifact.Tool.MotionSketchTool;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layers.Selection.Manager;
import Event.Bus;
import Time.Rational;
import Undo.UndoManager;

namespace Artifact {

W_OBJECT_IMPL(ArtifactMotionSketchTool)

class MotionSketchUndoCommand final : public UndoCommand {
 public:
  using Snapshot = std::map<int64_t, std::pair<float, float>>;

  MotionSketchUndoCommand(ArtifactAbstractLayerPtr layer, Snapshot before,
                          Snapshot after)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)) {}

  void undo() override { apply(before_); }
  void redo() override { apply(after_); }
  QString label() const override { return QStringLiteral("Motion Sketch"); }

 private:
  void apply(const Snapshot& snap) {
    auto layer = layer_.lock();
    if (!layer) return;
    auto& t3d = layer->transform3D();
    t3d.clearPositionKeyFrames();
    if (snap.empty()) return;
    for (const auto& [frame, xy] : snap) {
      ArtifactCore::RationalTime rt(frame, 24);
      t3d.setPosition(rt, xy.first, xy.second);
    }
    layer->setDirty(LayerDirtyFlag::Transform);
    layer->changed();
    if (auto* comp =
            static_cast<ArtifactAbstractComposition*>(layer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
    if (auto* mgr = UndoManager::instance()) {
      mgr->notifyAnythingChanged();
    }
  }

  ArtifactAbstractLayerWeak layer_;
  Snapshot before_;
  Snapshot after_;
};

class ArtifactMotionSketchTool::Impl {
public:
    bool active = false;
    bool sketching = false;

    // Sampling state
    std::vector<QPointF> sampledPoints;
    std::vector<double> sampledTimes; // seconds relative to sketch start
    QElapsedTimer sketchTimer;

    // Smoothing
    float smoothing = 0.5f;
    int minSamples = 2;
    double sampleInterval = 0.016; // ~60fps sampling

    // Target layer
    ArtifactAbstractLayerPtr targetLayer;

    // Undo snapshot
    using PositionSnapshot = MotionSketchUndoCommand::Snapshot;
    PositionSnapshot beforePositions;
};

ArtifactMotionSketchTool::ArtifactMotionSketchTool(QObject* parent)
    : QObject(parent), impl_(new Impl())
{
}

ArtifactMotionSketchTool::~ArtifactMotionSketchTool()
{
    delete impl_;
}

void ArtifactMotionSketchTool::activate()
{
    impl_->active = true;
}

void ArtifactMotionSketchTool::deactivate()
{
    impl_->active = false;
    if (impl_->sketching) {
        finishSketch();
    }
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
    impl_->targetLayer.reset();
}

bool ArtifactMotionSketchTool::isActive() const
{
    return impl_->active;
}

bool ArtifactMotionSketchTool::beginSketch(const QPointF& canvasPos, ArtifactAbstractLayerPtr layer)
{
    if (!impl_->active || !layer) return false;

    impl_->targetLayer = layer;
    impl_->sketching = true;
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
    impl_->sampledPoints.push_back(canvasPos);
    impl_->sampledTimes.push_back(0.0);
    impl_->sketchTimer.start();

    impl_->beforePositions.clear();
    auto& t3d = layer->transform3D();
    for (const auto& kt : t3d.getPositionKeyFrameTimes()) {
        const int64_t frame = kt.rescaledTo(24);
        impl_->beforePositions[frame] = {t3d.positionXAt(kt), t3d.positionYAt(kt)};
    }

    return true;
}

bool ArtifactMotionSketchTool::addSample(const QPointF& canvasPos)
{
    if (!impl_->sketching) return false;

    const double elapsed = impl_->sketchTimer.elapsed() / 1000.0;
    if (!impl_->sampledTimes.empty()) {
        const double dt = elapsed - impl_->sampledTimes.back();
        if (dt < impl_->sampleInterval) return false; // throttle
    }

    impl_->sampledPoints.push_back(canvasPos);
    impl_->sampledTimes.push_back(elapsed);
    return true;
}

bool ArtifactMotionSketchTool::finishSketch()
{
    if (!impl_->sketching) return false;
    impl_->sketching = false;

    auto layer = impl_->targetLayer;
    if (!layer || impl_->sampledPoints.size() < 2) return false;

    const size_t n = impl_->sampledPoints.size();

    // Apply smoothing (moving average)
    std::vector<QPointF> smoothPoints = impl_->sampledPoints;
    if (impl_->smoothing > 0.0f && n > 2) {
        const float s = std::clamp(impl_->smoothing, 0.0f, 1.0f);
        const int window = std::max(1, static_cast<int>(s * 5.0f));
        for (size_t i = 0; i < n; ++i) {
            float sumX = 0, sumY = 0;
            int count = 0;
            const int start = std::max(0, static_cast<int>(i) - window);
            const int end = std::min(static_cast<int>(n) - 1, static_cast<int>(i) + window);
            for (int j = start; j <= end; ++j) {
                sumX += static_cast<float>(impl_->sampledPoints[j].x());
                sumY += static_cast<float>(impl_->sampledPoints[j].y());
                ++count;
            }
            if (count > 0) {
                smoothPoints[i] = QPointF(sumX / count, sumY / count);
            }
        }
    }

    // Create keyframes on the layer's transform position.
    auto& t3d = layer->transform3D();
    const double fps = [&]() -> double {
        if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
            return std::max(1.0, static_cast<double>(comp->frameRate().framerate()));
        }
        return 24.0;
    }();

    const int startFrame = [&]() -> int {
        if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
            return static_cast<int>(comp->framePosition().framePosition());
        }
        return 0;
    }();

    for (size_t i = 0; i < n; ++i) {
        const double t = impl_->sampledTimes[i];
        const int frameNum = startFrame + static_cast<int>(std::llround(t * fps));
        const float x = static_cast<float>(smoothPoints[i].x());
        const float y = static_cast<float>(smoothPoints[i].y());
        RationalTime rt(frameNum, static_cast<int64_t>(fps));
        t3d.setPosition(rt, x, y);
    }

    // Capture after-state for undo
    MotionSketchUndoCommand::Snapshot afterPositions;
    for (const auto& kt : t3d.getPositionKeyFrameTimes()) {
        const int64_t frame = kt.rescaledTo(24);
        afterPositions[frame] = {t3d.positionXAt(kt), t3d.positionYAt(kt)};
    }

    if (auto* mgr = UndoManager::instance()) {
        mgr->push(std::make_unique<MotionSketchUndoCommand>(
            layer, impl_->beforePositions, std::move(afterPositions)));
    }

    // Notify
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }

    return true;
}

void ArtifactMotionSketchTool::cancelSketch()
{
    impl_->sketching = false;
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
}

bool ArtifactMotionSketchTool::isSketching() const
{
    return impl_->sketching;
}

void ArtifactMotionSketchTool::setSmoothing(float factor)
{
    impl_->smoothing = std::clamp(factor, 0.0f, 1.0f);
}

float ArtifactMotionSketchTool::smoothing() const
{
    return impl_->smoothing;
}

const std::vector<QPointF>& ArtifactMotionSketchTool::sampledPoints() const
{
    return impl_->sampledPoints;
}

} // namespace Artifact
