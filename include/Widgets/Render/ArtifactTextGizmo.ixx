module;
#include <QPointF>
#include <QRectF>
#include <QCursor>
#include <vector>
#include <memory>

export module Artifact.Widgets.TextGizmo;

import Artifact.Render.IRenderer;
import Artifact.Layer.Abstract;
import Text.GlyphLayout;
import Text.Animator;

namespace Artifact {

export class TextGizmo {
public:
    enum class HandleType {
        None,
        RangeStart,
        RangeEnd,
        Offset,
        CharacterSelect,
        AnchorPoint
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

private:
    ArtifactAbstractLayerPtr layer_;
    bool isDragging_ = false;
    HandleType activeHandle_ = HandleType::None;
    
    QPointF dragStartCanvasPos_;
    float dragStartValue_ = 0.0f;

    // 定数
    static constexpr float HANDLE_WIDTH = 4.0f;
    static constexpr float RANGE_LINE_HEIGHT = 40.0f;
};

} // namespace Artifact
