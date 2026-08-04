module;
#include <wobjectdefs.h>
#include <QObject>
#include <QPointF>
#include <QString>

export module Artifact.Tool.Brush;

import std;
import Artifact.Layer.Abstract;
import Artifact.Layer.Paint;
import FloatRGBA;

export namespace Artifact {

class ArtifactBrushTool : public QObject {
    W_OBJECT(ArtifactBrushTool)
public:
    explicit ArtifactBrushTool(QObject* parent = nullptr);
    ~ArtifactBrushTool();

    bool mousePressEvent(const ArtifactAbstractLayerPtr& layer,
                         const QPointF& canvasPos);
    bool mouseMoveEvent(const ArtifactAbstractLayerPtr& layer,
                        const QPointF& canvasPos);
    bool mouseReleaseEvent(const ArtifactAbstractLayerPtr& layer,
                           const QPointF& canvasPos);
    void cancelStroke(const ArtifactAbstractLayerPtr& layer);
    bool isDragging() const { return dragging_; }
    const std::vector<QPointF>& currentStrokePoints() const {
        return previewStrokePoints_;
    }
    const std::vector<QPointF>& lastStrokePoints() const {
        return lastStrokePoints_;
    }

    void setRadius(float radius) {
        radius_ = std::isfinite(radius) ? std::clamp(radius, 0.5f, 2500.0f) : 10.0f;
    }
    float radius() const { return radius_; }
    void setOpacity(float opacity) {
        opacity_ = std::isfinite(opacity) ? std::clamp(opacity, 0.0f, 1.0f) : 1.0f;
    }
    float opacity() const { return opacity_; }
    void setFlow(float flow) { flow_ = std::isfinite(flow) ? std::clamp(flow, 0.0f, 1.0f) : 1.0f; }
    float flow() const { return flow_; }
    void setHardness(float hardness) { hardness_ = std::isfinite(hardness) ? std::clamp(hardness, 0.0f, 1.0f) : 1.0f; }
    float hardness() const { return hardness_; }
    void setSpacing(float spacing) { spacing_ = std::isfinite(spacing) ? std::clamp(spacing, 0.01f, 10.0f) : 0.25f; }
    float spacing() const { return spacing_; }
    void setAngle(float angle) {
        angle_ = std::isfinite(angle) ? std::fmod(angle, 360.0f) : 0.0f;
        if (angle_ < 0.0f) angle_ += 360.0f;
    }
    float angle() const { return angle_; }
    void setRoundness(float roundness) { roundness_ = std::isfinite(roundness) ? std::clamp(roundness, 0.01f, 1.0f) : 1.0f; }
    float roundness() const { return roundness_; }
    void setSizeJitter(float jitter) { sizeJitter_ = std::isfinite(jitter) ? std::clamp(jitter, 0.0f, 1.0f) : 0.0f; }
    float sizeJitter() const { return sizeJitter_; }
    void setOpacityJitter(float jitter) { opacityJitter_ = std::isfinite(jitter) ? std::clamp(jitter, 0.0f, 1.0f) : 0.0f; }
    float opacityJitter() const { return opacityJitter_; }
    void setScatter(float scatter) { scatter_ = std::isfinite(scatter) ? std::clamp(scatter, 0.0f, 1.0f) : 0.0f; }
    float scatter() const { return scatter_; }
    void setAngleJitter(float jitter) { angleJitter_ = std::isfinite(jitter) ? std::clamp(jitter, 0.0f, 1.0f) : 0.0f; }
    float angleJitter() const { return angleJitter_; }
    void setRoundnessJitter(float jitter) { roundnessJitter_ = std::isfinite(jitter) ? std::clamp(jitter, 0.0f, 1.0f) : 0.0f; }
    float roundnessJitter() const { return roundnessJitter_; }
    void setFlowJitter(float jitter) { flowJitter_ = std::isfinite(jitter) ? std::clamp(jitter, 0.0f, 1.0f) : 0.0f; }
    float flowJitter() const { return flowJitter_; }
    void setPressure(float pressure) { pressure_ = std::isfinite(pressure) ? std::clamp(pressure, 0.0f, 1.0f) : 1.0f; }
    float pressure() const { return pressure_; }
    void setPressureAffectsSize(bool enabled) { pressureAffectsSize_ = enabled; }
    bool pressureAffectsSize() const { return pressureAffectsSize_; }
    void setPressureAffectsOpacity(bool enabled) { pressureAffectsOpacity_ = enabled; }
    bool pressureAffectsOpacity() const { return pressureAffectsOpacity_; }
    void setPressureAffectsFlow(bool enabled) { pressureAffectsFlow_ = enabled; }
    bool pressureAffectsFlow() const { return pressureAffectsFlow_; }
    void setTiltAffectsAngle(bool enabled) { tiltAffectsAngle_ = enabled; }
    bool tiltAffectsAngle() const { return tiltAffectsAngle_; }
    void setTiltAffectsRoundness(bool enabled) { tiltAffectsRoundness_ = enabled; }
    bool tiltAffectsRoundness() const { return tiltAffectsRoundness_; }
    void setTilt(float tiltX, float tiltY) {
        tiltX_ = std::isfinite(tiltX) ? std::clamp(tiltX, -60.0f, 60.0f) : 0.0f;
        tiltY_ = std::isfinite(tiltY) ? std::clamp(tiltY, -60.0f, 60.0f) : 0.0f;
    }
    float tiltX() const { return tiltX_; }
    float tiltY() const { return tiltY_; }
    void setColor(const FloatRGBA& color) {
        const auto safeChannel = [](const float value, const float fallback) {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
        };
        color_ = FloatRGBA(safeChannel(color.r(), 0.0f),
                           safeChannel(color.g(), 0.0f),
                           safeChannel(color.b(), 0.0f),
                           safeChannel(color.a(), 1.0f));
    }
    const FloatRGBA& color() const { return color_; }
    void setEraserMode(bool eraser) { eraserMode_ = eraser; }
    bool eraserMode() const { return eraserMode_; }
    void setRotoInputMode(bool enabled) { rotoInputMode_ = enabled; }
    bool rotoInputMode() const { return rotoInputMode_; }
    void setLastStrokeOnly(bool enabled) { lastStrokeOnly_ = enabled; }
    bool lastStrokeOnly() const { return lastStrokeOnly_; }
    void setEraserModeKind(int mode) {
        eraserModeKind_ = std::clamp(mode, 0, 2);
        lastStrokeOnly_ = eraserModeKind_ == 2;
    }
    int eraserModeKind() const { return eraserModeKind_; }
    void setCloneAligned(bool aligned) { cloneAligned_ = aligned; }
    bool cloneAligned() const { return cloneAligned_; }
    void setCloneTimeOffset(int offset) { cloneTimeOffset_ = std::clamp(offset, -10000, 10000); }
    int cloneTimeOffset() const { return cloneTimeOffset_; }

private:
    float radius_ = 10.0f;
    float opacity_ = 1.0f;
    float flow_ = 1.0f;
    float hardness_ = 1.0f;
    float spacing_ = 0.25f;
    float angle_ = 0.0f;
    float roundness_ = 1.0f;
    float sizeJitter_ = 0.0f;
    float opacityJitter_ = 0.0f;
    float scatter_ = 0.0f;
    float angleJitter_ = 0.0f;
    float roundnessJitter_ = 0.0f;
    float flowJitter_ = 0.0f;
    float pressure_ = 1.0f;
    bool pressureAffectsSize_ = true;
    bool pressureAffectsOpacity_ = true;
    bool pressureAffectsFlow_ = true;
    bool tiltAffectsAngle_ = true;
    bool tiltAffectsRoundness_ = true;
    float tiltX_ = 0.0f;
    float tiltY_ = 0.0f;
    FloatRGBA color_ = {0.0f, 0.0f, 0.0f, 1.0f};
    bool eraserMode_ = false;
    bool rotoInputMode_ = false;
    bool lastStrokeOnly_ = false;
    int eraserModeKind_ = 0;
    bool cloneAligned_ = true;
    int cloneTimeOffset_ = 0;
    bool dragging_ = false;
    bool undoRecorded_ = false;
    BrushStroke currentStroke_;
    std::vector<QPointF> previewStrokePoints_;
    std::vector<QPointF> lastStrokePoints_;
    std::vector<QPointF> activeStrokePoints_;
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::ArtifactBrushTool)
