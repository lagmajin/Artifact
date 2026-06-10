module;
#include <utility>
#include <vector>
#include <cmath>
#include <QPointF>
#include <QElapsedTimer>
#include <wobjectimpl.h>

module Artifact.Tool.MotionSketchTool;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;
import Artifact.Layers.Selection.Manager;
import Event.Bus;

namespace Artifact {

W_OBJECT_IMPL(ArtifactMotionSketchTool)

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
    struct PositionSnapshot {
        float x, y;
    };
    std::vector<PositionSnapshot> beforePositions;
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

    auto layer = impl_->targetLayer.lock();
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

    // Create keyframes on the layer's transform position
    auto& t3d = layer->transform3D();
    const double fps = 24.0;

    // Clear existing position keyframes for the frame range we'll use
    const int startFrame = 0;
    const int endFrame = startFrame + static_cast<int>(n * fps * impl_->sampleInterval);

    for (size_t i = 0; i < n; ++i) {
        const double t = impl_->sampledTimes[i];
        const int frameNum = startFrame + static_cast<int>(std::llround(t * fps));
        const float x = static_cast<float>(smoothPoints[i].x());
        const float y = static_cast<float>(smoothPoints[i].y());
        ArtifactCore::RationalTime rt(frameNum, static_cast<int>(fps));
        t3d.setPosition(rt, x, y);
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
