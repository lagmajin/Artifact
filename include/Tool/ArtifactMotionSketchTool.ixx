module;
#include <utility>
#include <vector>
#include <QObject>
#include <QPointF>
#include <wobjectdefs.h>

export module Artifact.Tool.MotionSketchTool;

import Artifact.Layer.Abstract;

export namespace Artifact {

class ArtifactMotionSketchTool : public QObject {
    W_OBJECT(ArtifactMotionSketchTool)
public:
    explicit ArtifactMotionSketchTool(QObject* parent = nullptr);
    ~ArtifactMotionSketchTool();

    void activate();
    void deactivate();
    bool isActive() const;

    // Sketch lifecycle
    bool beginSketch(const QPointF& canvasPos, ArtifactAbstractLayerPtr layer);
    bool addSample(const QPointF& canvasPos);
    bool finishSketch();
    void cancelSketch();
    bool isSketching() const;

    // Settings
    void setSmoothing(float factor);
    float smoothing() const;

    // Debug/preview access
    const std::vector<QPointF>& sampledPoints() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
