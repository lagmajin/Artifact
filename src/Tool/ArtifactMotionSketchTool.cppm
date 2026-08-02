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

namespace {
bool isFinitePoint(const QPointF& point) {
    return std::isfinite(point.x()) && std::isfinite(point.y());
}
}

W_OBJECT_IMPL(ArtifactMotionSketchTool)

class MotionSketchUndoCommand final : public UndoCommand {
 public:
  using Snapshot = std::map<int64_t, std::pair<float, float>>;

  MotionSketchUndoCommand(ArtifactAbstractLayerPtr layer, Snapshot before,
                          Snapshot after, int64_t frameRate)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)),
        frameRate_(std::max<int64_t>(1, frameRate)) {}

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
      ArtifactCore::RationalTime rt(frame, frameRate_);
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
  int64_t frameRate_ = 24;
};

class ArtifactMotionSketchTool::Impl {
public:
    bool active = false;
    bool sketching = false;
    int64_t sketchStartFrame = 0;

    // Sampling state
    std::vector<QPointF> sampledPoints;
    std::vector<double> sampledTimes; // seconds relative to sketch start
    QElapsedTimer sketchTimer;

    // Smoothing
    float smoothing = 0.5f;
    int minSamples = 2;
    double sampleInterval = 0.016; // ~60fps sampling
    bool showWireframe = false;
    bool showBackground = true;

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
    if (!impl_->active || !layer || !isFinitePoint(canvasPos)) return false;

    impl_->targetLayer = layer;
    impl_->sketching = true;
    if (auto *comp = static_cast<ArtifactAbstractComposition *>(
            layer->composition())) {
        impl_->sketchStartFrame = comp->framePosition().framePosition();
    } else {
        impl_->sketchStartFrame = 0;
    }
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
    impl_->sampledPoints.push_back(canvasPos);
    impl_->sampledTimes.push_back(0.0);
    impl_->sketchTimer.start();

    impl_->beforePositions.clear();
    const int64_t snapshotFrameRate = [&]() -> int64_t {
        if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
            return std::max<int64_t>(1, static_cast<int64_t>(comp->frameRate().framerate()));
        }
        return 24;
    }();
    auto& t3d = layer->transform3D();
    for (const auto& kt : t3d.getPositionKeyFrameTimes()) {
        const int64_t frame = kt.rescaledTo(snapshotFrameRate);
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
    const int64_t sketchStartFrame = impl_->sketchStartFrame;
    impl_->sketchStartFrame = 0;

    auto layer = impl_->targetLayer;
    if (!layer || impl_->sampledPoints.size() < 2) return false;

    if (impl_->sampledPoints.size() != impl_->sampledTimes.size()) {
        return false;
    }
    for (size_t i = 0; i < impl_->sampledPoints.size(); ++i) {
        if (!isFinitePoint(impl_->sampledPoints[i]) ||
            !std::isfinite(impl_->sampledTimes[i]) ||
            impl_->sampledTimes[i] < 0.0) {
            return false;
        }
    }

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

    for (size_t i = 0; i < n; ++i) {
        const double t = impl_->sampledTimes[i];
        const int64_t frameNum =
            sketchStartFrame + static_cast<int64_t>(std::llround(t * fps));
        const float x = static_cast<float>(smoothPoints[i].x());
        const float y = static_cast<float>(smoothPoints[i].y());
        RationalTime rt(frameNum, static_cast<int64_t>(fps));
        t3d.setPosition(rt, x, y);
    }

    // Capture after-state for undo
    MotionSketchUndoCommand::Snapshot afterPositions;
    for (const auto& kt : t3d.getPositionKeyFrameTimes()) {
        const int64_t frame = kt.rescaledTo(static_cast<int64_t>(fps));
        afterPositions[frame] = {t3d.positionXAt(kt), t3d.positionYAt(kt)};
    }

    if (auto* mgr = UndoManager::instance()) {
        mgr->push(std::make_unique<MotionSketchUndoCommand>(
            layer, impl_->beforePositions, std::move(afterPositions),
            static_cast<int64_t>(fps)));
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
    impl_->sketchStartFrame = 0;
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
}

bool ArtifactMotionSketchTool::isSketching() const
{
    return impl_->sketching;
}

void ArtifactMotionSketchTool::setSmoothing(float factor)
{
    impl_->smoothing = std::isfinite(factor)
        ? std::clamp(factor, 0.0f, 1.0f)
        : 0.5f;
}

void ArtifactMotionSketchTool::setSampleRate(float framesPerSecond)
{
    const double fps = std::isfinite(framesPerSecond)
        ? std::clamp(static_cast<double>(framesPerSecond), 1.0, 60.0)
        : 60.0;
    impl_->sampleInterval = 1.0 / fps;
}

void ArtifactMotionSketchTool::setShowWireframe(bool enabled)
{
    impl_->showWireframe = enabled;
}

bool ArtifactMotionSketchTool::showWireframe() const
{
    return impl_->showWireframe;
}

void ArtifactMotionSketchTool::setShowBackground(bool enabled)
{
    impl_->showBackground = enabled;
}

bool ArtifactMotionSketchTool::showBackground() const
{
    return impl_->showBackground;
}

float ArtifactMotionSketchTool::sampleRate() const
{
    return static_cast<float>(1.0 / std::max(0.001, impl_->sampleInterval));
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
