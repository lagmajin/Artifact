module;
#include <utility>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QImage>
#include <wobjectdefs.h>

export module Artifact.Tool.PuppetTool;

import Utils.Id;
import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;

export namespace Artifact {

class ArtifactPuppetTool : public QObject {
    W_OBJECT(ArtifactPuppetTool)
public:
    explicit ArtifactPuppetTool(QObject* parent = nullptr);
    ~ArtifactPuppetTool();

    void activate();
    void deactivate();
    bool isActive() const;

    // Pin management
    bool addPin(const LayerID& layerId, const QPointF& canvasPos);
    bool removePin(const QString& pinId);
    bool movePin(const QString& pinId, const QPointF& canvasPos);
    QString hitTestPin(const QPointF& canvasPos, float threshold = 12.0f) const;

    // Deformation
    void deformLayer(const LayerID& layerId, ArtifactIRenderer* renderer);
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
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
