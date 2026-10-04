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
    void setSampleRate(float framesPerSecond);
    float sampleRate() const;
    void setShowWireframe(bool enabled);
    bool showWireframe() const;
    void setShowBackground(bool enabled);
    bool showBackground() const;

    // Pen tablet input
    void setPressure(float pressure);
    float pressure() const;
    void setTilt(float tiltX, float tiltY);
    float tiltX() const;
    float tiltY() const;

    // Pen pressure response. When enabled, the recorded pen pressure is
    // converted into keyframes on the layer's "layer.opacity" property so a
    // drawn motion can carry an authored opacity envelope.
    void setPressureAffectsOpacity(bool enabled);
    bool pressureAffectsOpacity() const;

    // Debug/preview access
    const std::vector<QPointF>& sampledPoints() const;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
