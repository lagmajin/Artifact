module;
#include <utility>
#include <memory>
#include <QObject>
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <wobjectdefs.h>
export module Artifact.Widgets.Gizmo3D;


import Artifact.Render.IRenderer;
import Color.Float;
import Math.Vec;

export namespace Artifact {

enum class GizmoMode {
    Move,
    Rotate,
    Scale,
    Full,
    // Anchor-only manipulator: a single draggable point at the layer anchor.
    // Owned by this gizmo so projected-frame layers (image / solid / 3D plane)
    // can move their anchor without reviving the legacy 2D gizmo's press path.
    AnchorPoint
};

enum class GizmoOperation {
    None,
    Translate,
    Rotate,
    Scale,
    Anchor
};

enum class GizmoSpace {
    World,
    Local,
    View
};

enum class GizmoAxis {
    None,
    X,
    Y,
    Z,
    XY,
    YZ,
    XZ,
    Screen,
    // Single anchor point handle (Mode::AnchorPoint only).
    Anchor
};

struct Ray {
    QVector3D origin;
    QVector3D direction;
};

// Typed boundary for rays consumed by the 3D gizmo. The controller supplies
// an unprojected near-plane world point and a normalized world-space direction.
// The implementation keeps its existing QVector3D geometry after converting
// once on API entry.
struct WorldRay {
    ArtifactCore::Coordinates::WorldPoint3 origin;
    ArtifactCore::Coordinates::WorldVector3 direction;
};

export class Artifact3DGizmo : public QObject {
    W_OBJECT(Artifact3DGizmo)
public:
    explicit Artifact3DGizmo(QObject* parent = nullptr);
    virtual ~Artifact3DGizmo();

    void setMode(GizmoMode mode);
    GizmoMode mode() const { return mode_; }
    void setSpace(GizmoSpace space) { space_ = space; }
    GizmoSpace space() const { return space_; }
    // View-camera basis directions expressed in world coordinates.
    void setViewBasis(ArtifactCore::Coordinates::WorldVector3 xAxis,
                      ArtifactCore::Coordinates::WorldVector3 yAxis,
                      ArtifactCore::Coordinates::WorldVector3 zAxis);

    void setTransform(ArtifactCore::Coordinates::WorldPoint3 position,
                      ArtifactCore::Units::EulerDegrees3 rotation);
    void resetState();
    // The object's local axes expressed in world coordinates.
    void setLocalBasis(ArtifactCore::Coordinates::WorldVector3 xAxis,
                       ArtifactCore::Coordinates::WorldVector3 yAxis,
                       ArtifactCore::Coordinates::WorldVector3 zAxis);
    ArtifactCore::Coordinates::WorldPoint3 position() const;
    ArtifactCore::Units::EulerDegrees3 rotation() const;
    void setScale(ArtifactCore::Units::Scale3 scale);
    ArtifactCore::Units::Scale3 scale() const;
    // Bounds use the active target layer's local coordinate space.
    void setBoundingBox(ArtifactCore::Coordinates::LayerLocalPoint3 minBounds,
                        ArtifactCore::Coordinates::LayerLocalPoint3 maxBounds);
    void clearBoundingBox();
    bool hasBoundingBox() const { return boundingBoxEnabled_; }
    void setDepthEnabled(bool enabled) { depthEnabled_ = enabled; }
    bool depthEnabled() const { return depthEnabled_; }
    void setInteractionModifiers(bool snapEnabled, bool fineAdjustment) {
        snapEnabled_ = snapEnabled;
        fineAdjustment_ = fineAdjustment;
    }
    
    // Hit testing
    GizmoAxis hitTest(const WorldRay& ray, const QMatrix4x4& view, const QMatrix4x4& proj);
    
    // Interaction
    void beginDrag(GizmoAxis axis, const WorldRay& ray, float axisDirectionSign = 1.0f);
    void beginDrag(GizmoAxis axis, const WorldRay& ray,
                   const QVector3D& scaleSigns);
    void constrainDrag(GizmoAxis axis, const WorldRay& currentRay);
    void setNumericInput(ArtifactCore::Units::WorldLength value);
    void setNumericInput(ArtifactCore::Units::Degrees value);
    void setNumericInput(ArtifactCore::Units::ScaleFactor factor);
    void setNumericPlanarScaleInput(ArtifactCore::Units::ScaleFactor factor);
    void clearNumericInput();
    void updateDrag(const WorldRay& ray);
    void endDrag();
    bool isDragging() const { return activeAxis_ != GizmoAxis::None; }
    GizmoAxis activeAxis() const { return activeAxis_; }
    // World-space offset of an AnchorPoint drag, valid while dragging with
    // activeAxis() == GizmoAxis::Anchor.  The controller converts this into
    // layer-local anchor coordinates; the gizmo never mutates position for it.
    ArtifactCore::Coordinates::WorldVector3 anchorDragDelta() const;
    ArtifactCore::Coordinates::WorldVector3 dragAxisDirection() const;
    GizmoAxis hoverAxis() const { return hoverAxis_; }
    float hoverAxisDirectionSign() const { return hoverAxisDirectionSign_; }
    QVector3D hoverScaleAxes() const { return hoverScaleAxes_; }
    QVector3D hoverScaleSigns() const { return hoverScaleSigns_; }
    bool isBoundingBoxDragging() const;
    GizmoOperation activeOperation() const { return activeOperation_; }
    GizmoOperation hoverOperation() const { return hoverOperation_; }

    // Rendering
    void draw(ArtifactIRenderer* renderer, const QMatrix4x4& view, const QMatrix4x4& proj,
              float viewportWidth = 0.0f, float viewportHeight = 0.0f);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    
    GizmoMode mode_ = GizmoMode::Move;
    GizmoSpace space_ = GizmoSpace::World;
    GizmoAxis activeAxis_ = GizmoAxis::None;
    GizmoAxis hoverAxis_ = GizmoAxis::None;
    float hoverAxisDirectionSign_ = 1.0f;
    QVector3D hoverScaleAxes_;
    QVector3D hoverScaleSigns_{1.0f, 1.0f, 1.0f};
    GizmoOperation activeOperation_ = GizmoOperation::None;
    GizmoOperation hoverOperation_ = GizmoOperation::None;
    bool depthEnabled_ = true;
    bool boundingBoxEnabled_ = false;
    ArtifactCore::Coordinates::LayerLocalPoint3 boundingBoxMin_{};
    ArtifactCore::Coordinates::LayerLocalPoint3 boundingBoxMax_{};
    bool snapEnabled_ = false;
    bool fineAdjustment_ = false;
    bool fullModeDrag_ = false;
};

} // namespace Artifact
