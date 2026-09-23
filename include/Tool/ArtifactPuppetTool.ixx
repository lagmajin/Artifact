module;
#include <utility>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QImage>
#include <QMatrix4x4>
#include <QJsonObject>
#include <wobjectdefs.h>

export module Artifact.Tool.PuppetTool;

import Utils.Id;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Render.IRenderer;

export namespace Artifact {

enum class Deformation2DMode : int {
    Pins = 0,
    Grid = 1
};

class ArtifactPuppetTool : public QObject {
    W_OBJECT(ArtifactPuppetTool)
public:
    explicit ArtifactPuppetTool(QObject* parent = nullptr);
    ~ArtifactPuppetTool();

    void activate();
    void deactivate();
    bool isActive() const;

    // Pin management
    void ensureLayerLoaded(const LayerID& layerId);
    void ensureLayerLoaded(const LayerID& layerId, ArtifactAbstractLayer* layer);
    void persistLayerData(const LayerID& layerId);
    bool restoreLayerData(const LayerID& layerId, const QJsonObject& state,
                          ArtifactAbstractLayer* layer = nullptr);
    bool setDeformation2DMode(const LayerID& layerId, Deformation2DMode mode,
                              int columns = 5, int rows = 5);
    Deformation2DMode deformation2DMode(const LayerID& layerId) const;
    bool addPin(const LayerID& layerId, const QPointF& canvasPos);
    bool removePin(const QString& pinId);
    bool movePin(const QString& pinId, const QPointF& canvasPos);
    bool movePinAtFrame(const QString& pinId, const QPointF& canvasPos);
    bool commitPinPositionAtFrame(const QString& pinId,
                                  const QPointF& canvasPos);
    bool restorePinPositionAnimation(const LayerID& layerId,
                                     const QString& pinId,
                                     const QJsonObject& snapshot,
                                     const QPointF& position);
    QJsonObject pinPositionAnimationSnapshot(const LayerID& layerId,
                                             const QString& pinId) const;
    void discardPinPositionAnimationProperties(const LayerID& layerId,
                                               const QString& pinId);
    void evaluatePinPositionsAtCurrentFrame(const LayerID& layerId,
                                            ArtifactAbstractLayer* layer = nullptr);
    QPointF pinPosition(const QString& pinId) const;
    LayerID pinLayerId(const QString& pinId) const;
    float pinRotation(const QString& pinId) const;
    void setPinRotation(const QString& pinId, float degrees);
    float pinWeight(const QString& pinId) const;
    void setPinWeight(const QString& pinId, float weight);
    float pinDepth(const QString& pinId) const;
    void setPinDepth(const QString& pinId, float depth);
    bool isProportionalEditingEnabled() const;
    void setProportionalEditingEnabled(bool enabled);
    float proportionalEditRadius() const;
    void setProportionalEditRadius(float radius);
    QString hitTestPin(const QPointF& canvasPos, float threshold = 12.0f) const;

    // Deformation
    void deformLayer(const LayerID& layerId, ArtifactIRenderer* renderer);
    bool renderDeformedLayer(ArtifactIRenderer* renderer,
                             ArtifactImageLayer* imageLayer,
                             const QMatrix4x4& transform, float opacity);
    QPointF mapDeformationPoint(ArtifactAbstractLayer* layer,
                                const QPointF& localPoint);
    bool prepareLayerDeformation(ArtifactAbstractLayer* layer);
    void clearPins(const LayerID& layerId);

    // Selection state
    QString selectedPinId() const;
    void setSelectedPinId(const QString& pinId);

    // Render overlay (draw pins and mesh)
    void renderOverlay(ArtifactIRenderer* renderer, const LayerID& layerId) const;

    // Pin type switching
    void setPinTypeFor(const QString& pinId, int type);
    int pinTypeFor(const QString& pinId) const;

private:
    void rebaseLayerPins(const LayerID& layerId, ArtifactAbstractLayer* layer);
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
