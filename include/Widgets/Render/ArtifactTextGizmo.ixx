module;
#include <utility>
#include <vector>
#include <memory>
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <QString>
export module Artifact.Widgets.TextGizmo;


import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Artifact.Layer.Text;
import Text.GlyphLayout;
import Text.Animator;
import Property.Abstract;

namespace Artifact {

export class TextGizmo {
public:
    enum class HandleType {
        None,
        RangeStart,
        RangeEnd,
        RangeOffset,
        Offset,
        CharacterSelect,
        AnchorPoint,
        // Text box bounds editing
        BoxLeft,
        BoxRight,
        BoxTop,
        BoxBottom,
        BoxCornerTopLeft,
        BoxCornerTopRight,
        BoxCornerBottomLeft,
        BoxCornerBottomRight
    };

    TextGizmo();
    ~TextGizmo();

    void setLayer(ArtifactAbstractLayerPtr layer);
    void draw(ArtifactIRenderer* renderer);
    
    HandleType hitTest(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;
    Qt::CursorShape cursorShapeForViewportPos(const QPointF& viewportPos, ArtifactIRenderer* renderer) const;

    bool handleMousePress(const QPointF& viewportPos, ArtifactIRenderer* renderer);
    bool handleMouseMove(const QPointF& viewportPos, ArtifactIRenderer* renderer);
    void handleMouseRelease();

    bool isDragging() const { return isDragging_; }
    HandleType activeHandle() const { return activeHandle_; }

private:
    ArtifactAbstractLayerPtr layer_;
    bool isDragging_ = false;
    HandleType activeHandle_ = HandleType::None;
    
    QPointF dragStartCanvasPos_;
    QPointF dragStartLayerPosition_;
    QRectF dragStartBounds_;
    float dragStartValue_ = 0.0f;
    float dragCurrentValue_ = 0.0f;
    int dragAnimatorIndex_ = -1;
    QString dragPropertyPath_;
    std::vector<ArtifactCore::KeyFrame> dragBeforeKeyframes_;
    bool dragValueChanged_ = false;

    // 定数
    static constexpr float HANDLE_WIDTH = 4.0f;
    static constexpr float RANGE_LINE_HEIGHT = 40.0f;
};

} // namespace Artifact
