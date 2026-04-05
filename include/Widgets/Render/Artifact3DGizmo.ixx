module;
#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <memory>
#include <wobjectdefs.h>

export module Artifact.Widgets.Gizmo3D;

import Artifact.Render.IRenderer;
import Color.Float;

export namespace Artifact {

enum class GizmoMode {
    Move,
    Rotate,
    Scale
};

enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
    XY,
    YZ,
    XZ,
    Screen
};

struct Ray {
    QVector3D origin;
    QVector3D direction;
};

class Artifact3DGizmo : public QObject {
    W_OBJECT(Artifact3DGizmo)
public:
    explicit Artifact3DGizmo(QObject* parent = nullptr);
    virtual ~Artifact3DGizmo();

    void setMode(GizmoMode mode) { mode_ = mode; }
    GizmoMode mode() const { return mode_; }

    void setTransform(const QVector3D& position, const QVector3D& rotation);
    QVector3D position() const;
    QVector3D rotation() const;
    void setScale(const QVector3D& scale);
    QVector3D scale() const;
    void setDepthEnabled(bool enabled) { depthEnabled_ = enabled; }
    bool depthEnabled() const { return depthEnabled_; }
    
    // Hit testing
    GizmoAxis hitTest(const Ray& ray, const QMatrix4x4& view, const QMatrix4x4& proj);
    
    // Interaction
    void beginDrag(GizmoAxis axis, const Ray& ray);
    void updateDrag(const Ray& ray);
    void endDrag();
    bool isDragging() const { return activeAxis_ != GizmoAxis::None; }
    GizmoAxis activeAxis() const { return activeAxis_; }
    GizmoAxis hoverAxis() const { return hoverAxis_; }

    // Rendering
    void draw(ArtifactIRenderer* renderer, const QMatrix4x4& view, const QMatrix4x4& proj);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    GizmoMode mode_ = GizmoMode::Move;
    GizmoAxis activeAxis_ = GizmoAxis::None;
    GizmoAxis hoverAxis_ = GizmoAxis::None;
    bool depthEnabled_ = true;
};

} // namespace Artifact
