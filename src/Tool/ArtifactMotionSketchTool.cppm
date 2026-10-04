module;
#include <utility>
#include <vector>
#include <map>
#include <cmath>
#include <QPointF>
#include <QElapsedTimer>
#include <wobjectimpl.h>
#include <algorithm>
#include <memory>

module Artifact.Tool.MotionSketchTool;

import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layers.Selection.Manager;
import Event.Bus;
import Property.Abstract;
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
  using OpacitySnapshot = std::map<int64_t, float>;

  MotionSketchUndoCommand(ArtifactAbstractLayerPtr layer, Snapshot before,
                          Snapshot after, int64_t frameRate)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)),
        frameRate_(std::max<int64_t>(1, frameRate)) {}

  MotionSketchUndoCommand(ArtifactAbstractLayerPtr layer, Snapshot before,
                          Snapshot after, OpacitySnapshot opacityBefore,
                          OpacitySnapshot opacityAfter, int64_t frameRate)
      : layer_(layer), before_(std::move(before)), after_(std::move(after)),
        opacityBefore_(std::move(opacityBefore)),
        opacityAfter_(std::move(opacityAfter)),
        frameRate_(std::max<int64_t>(1, frameRate)),
        touchesOpacity_(true) {}

  void undo() override { lastOperationSucceeded_ = apply(before_, opacityBefore_); }
  void redo() override { lastOperationSucceeded_ = apply(after_, opacityAfter_); }
  bool lastOperationSucceeded() const override { return lastOperationSucceeded_; }
  QString label() const override { return QStringLiteral("Motion Sketch"); }

 private:
  bool apply(const Snapshot& snap, const OpacitySnapshot& opacitySnap) {
    auto layer = layer_.lock();
    if (!layer) return false;
    auto& t3d = layer->transform3D();
    t3d.clearPositionKeyFrames();
    for (const auto& [frame, xy] : snap) {
      ArtifactCore::RationalTime rt(frame, frameRate_);
      t3d.setPosition(rt, xy.first, xy.second);
    }
    if (touchesOpacity_) {
      applyOpacity(layer, opacitySnap);
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
    return true;
  }

  void applyOpacity(const ArtifactAbstractLayerPtr& layer,
                    const OpacitySnapshot& opacitySnap) {
    if (!layer) return;
    auto* prop = layer->getProperty(QStringLiteral("layer.opacity"));
    if (!prop) return;
    prop->clearKeyFrames();
    for (const auto& [frame, value] : opacitySnap) {
      prop->addKeyFrame(ArtifactCore::RationalTime(frame, frameRate_), value);
    }
  }

  ArtifactAbstractLayerWeak layer_;
  Snapshot before_;
  Snapshot after_;
  OpacitySnapshot opacityBefore_;
  OpacitySnapshot opacityAfter_;
  int64_t frameRate_ = 24;
  bool touchesOpacity_ = false;
  bool lastOperationSucceeded_ = true;
};

class ArtifactMotionSketchTool::Impl {
public:
    bool active = false;
    bool sketching = false;
    int64_t sketchStartFrame = 0;

    // Sampling state
    std::vector<QPointF> sampledPoints;
    std::vector<double> sampledTimes; // seconds relative to sketch start
    std::vector<float> sampledPressures; // pen pressure per accepted sample
    QElapsedTimer sketchTimer;

    // Pen tablet state
    float pressure = 1.0f;
    float tiltX = 0.0f;
    float tiltY = 0.0f;
    bool pressureAffectsOpacity = false;

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
    MotionSketchUndoCommand::OpacitySnapshot beforeOpacities;
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
    impl_->sampledPressures.clear();
    impl_->sampledPoints.push_back(canvasPos);
    impl_->sampledTimes.push_back(0.0);
    impl_->sampledPressures.push_back(impl_->pressure);
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

    impl_->beforeOpacities.clear();
    if (auto* opacityProp = layer->getProperty(QStringLiteral("layer.opacity"))) {
        for (const auto& key : opacityProp->getKeyFrames()) {
            const int64_t frame = key.time.rescaledTo(snapshotFrameRate);
            impl_->beforeOpacities[frame] = key.value.toFloat();
        }
    }

    return true;
}

bool ArtifactMotionSketchTool::addSample(const QPointF& canvasPos)
{
    if (!impl_->sketching || !isFinitePoint(canvasPos)) return false;

    const double elapsed = impl_->sketchTimer.elapsed() / 1000.0;
    if (!impl_->sampledTimes.empty()) {
        const double dt = elapsed - impl_->sampledTimes.back();
        if (dt < impl_->sampleInterval) return false; // throttle
    }

    impl_->sampledPoints.push_back(canvasPos);
    impl_->sampledTimes.push_back(elapsed);
    impl_->sampledPressures.push_back(impl_->pressure);
    return true;
}

bool ArtifactMotionSketchTool::finishSketch()
{
    if (!impl_->sketching) return false;
    impl_->sketching = false;
    const int64_t sketchStartFrame = impl_->sketchStartFrame;
    impl_->sketchStartFrame = 0;

    auto layer = impl_->targetLayer;
    if (!layer || impl_->sampledPoints.size() < 2) {
        impl_->sampledPoints.clear();
        impl_->sampledTimes.clear();
        impl_->sampledPressures.clear();
        impl_->targetLayer.reset();
        return false;
    }

    if (impl_->sampledPoints.size() != impl_->sampledTimes.size()) {
        impl_->sampledPoints.clear();
        impl_->sampledTimes.clear();
        impl_->sampledPressures.clear();
        impl_->targetLayer.reset();
        return false;
    }
    for (size_t i = 0; i < impl_->sampledPoints.size(); ++i) {
        if (!isFinitePoint(impl_->sampledPoints[i]) ||
            !std::isfinite(impl_->sampledTimes[i]) ||
            impl_->sampledTimes[i] < 0.0) {
            impl_->sampledPoints.clear();
            impl_->sampledTimes.clear();
            impl_->sampledPressures.clear();
            impl_->targetLayer.reset();
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

    // Convert recorded pen pressure into keyframes on "layer.opacity".
    // Pen hardware that reports no pressure yields a flat envelope that leaves
    // the existing opacity untouched in meaning, so this stays a no-op when the
    // response is disabled or the property is unavailable.
    bool wroteOpacity = false;
    auto* opacityProp = layer->getProperty(QStringLiteral("layer.opacity"));
    if (impl_->pressureAffectsOpacity && opacityProp &&
        impl_->sampledPressures.size() == n) {
        const bool pressureLooksActive =
            std::any_of(impl_->sampledPressures.begin(), impl_->sampledPressures.end(),
                        [](float p) { return std::isfinite(p) && p < 0.995f; });
        if (pressureLooksActive) {
            for (size_t i = 0; i < n; ++i) {
                const double t = impl_->sampledTimes[i];
                const int64_t frameNum =
                    sketchStartFrame + static_cast<int64_t>(std::llround(t * fps));
                float p = impl_->sampledPressures[i];
                if (!std::isfinite(p)) p = 1.0f;
                p = std::clamp(p, 0.0f, 1.0f);
                opacityProp->addKeyFrame(
                    RationalTime(frameNum, static_cast<int64_t>(fps)),
                    static_cast<double>(p));
            }
            // Restore full opacity on the following frame so the sketch does not
            // leave the layer faded at the last sampled frame.
            const double lastT = impl_->sampledTimes.back();
            const int64_t tailFrame =
                sketchStartFrame +
                static_cast<int64_t>(std::llround(lastT * fps)) + 1;
            opacityProp->addKeyFrame(
                RationalTime(tailFrame, static_cast<int64_t>(fps)), 1.0);
            wroteOpacity = true;
        }
    }

    // Capture after-state for undo
    MotionSketchUndoCommand::Snapshot afterPositions;
    for (const auto& kt : t3d.getPositionKeyFrameTimes()) {
        const int64_t frame = kt.rescaledTo(static_cast<int64_t>(fps));
        afterPositions[frame] = {t3d.positionXAt(kt), t3d.positionYAt(kt)};
    }

    MotionSketchUndoCommand::OpacitySnapshot afterOpacities;
    if (wroteOpacity) {
        for (const auto& key : opacityProp->getKeyFrames()) {
            const int64_t frame = key.time.rescaledTo(static_cast<int64_t>(fps));
            afterOpacities[frame] = key.value.toFloat();
        }
    } else {
        afterOpacities = impl_->beforeOpacities;
    }

    auto makeCommand = [&]() -> std::unique_ptr<UndoCommand> {
        if (wroteOpacity) {
            return std::make_unique<MotionSketchUndoCommand>(
                layer, impl_->beforePositions, afterPositions,
                impl_->beforeOpacities, afterOpacities,
                static_cast<int64_t>(fps));
        }
        return std::make_unique<MotionSketchUndoCommand>(
            layer, impl_->beforePositions, std::move(afterPositions),
            static_cast<int64_t>(fps));
    };

    if (auto* mgr = UndoManager::instance();
        mgr && !mgr->push(makeCommand())) {
            t3d.clearPositionKeyFrames();
            for (const auto& [frame, xy] : impl_->beforePositions) {
                t3d.setPosition(
                    RationalTime(frame, static_cast<int64_t>(fps)),
                    xy.first, xy.second);
            }
            if (wroteOpacity && opacityProp) {
                opacityProp->clearKeyFrames();
                for (const auto& [frame, value] : impl_->beforeOpacities) {
                    opacityProp->addKeyFrame(
                        RationalTime(frame, static_cast<int64_t>(fps)), value);
                }
            }
            layer->setDirty(LayerDirtyFlag::Transform);
            return false;
    }

    // Notify
    if (auto* comp = static_cast<ArtifactAbstractComposition*>(layer->composition())) {
        ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
            LayerChangedEvent{comp->id().toString(), layer->id().toString(),
                              LayerChangedEvent::ChangeType::Modified});
    }

    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
    impl_->sampledPressures.clear();
    impl_->targetLayer.reset();

    return true;
}

void ArtifactMotionSketchTool::cancelSketch()
{
    impl_->sketching = false;
    impl_->sketchStartFrame = 0;
    impl_->sampledPoints.clear();
    impl_->sampledTimes.clear();
    impl_->sampledPressures.clear();
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

void ArtifactMotionSketchTool::setPressure(float pressure)
{
    impl_->pressure = std::isfinite(pressure)
        ? std::clamp(pressure, 0.0f, 1.0f)
        : 1.0f;
}

float ArtifactMotionSketchTool::pressure() const
{
    return impl_->pressure;
}

void ArtifactMotionSketchTool::setTilt(float tiltX, float tiltY)
{
    impl_->tiltX = std::isfinite(tiltX) ? std::clamp(tiltX, -1.0f, 1.0f) : 0.0f;
    impl_->tiltY = std::isfinite(tiltY) ? std::clamp(tiltY, -1.0f, 1.0f) : 0.0f;
}

float ArtifactMotionSketchTool::tiltX() const
{
    return impl_->tiltX;
}

float ArtifactMotionSketchTool::tiltY() const
{
    return impl_->tiltY;
}

void ArtifactMotionSketchTool::setPressureAffectsOpacity(bool enabled)
{
    impl_->pressureAffectsOpacity = enabled;
}

bool ArtifactMotionSketchTool::pressureAffectsOpacity() const
{
    return impl_->pressureAffectsOpacity;
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
