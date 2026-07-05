module;
#include <wobjectdefs.h>
#include <QObject>
#include <QPointF>
#include <QString>

export module Artifact.Tool.Brush;

import std;
import Artifact.Layer.Abstract;
import Artifact.Layer.Paint;

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

    void setRadius(float radius) { radius_ = radius; }
    float radius() const { return radius_; }
    void setOpacity(float opacity) { opacity_ = opacity; }
    float opacity() const { return opacity_; }
    void setEraserMode(bool eraser) { eraserMode_ = eraser; }
    bool eraserMode() const { return eraserMode_; }

private:
    float radius_ = 10.0f;
    float opacity_ = 1.0f;
    bool eraserMode_ = false;
    bool dragging_ = false;
    BrushStroke currentStroke_;
};

} // namespace Artifact

W_REGISTER_ARGTYPE(Artifact::ArtifactBrushTool)
