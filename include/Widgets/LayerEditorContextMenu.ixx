module;

#include <QPoint>
#include <QPointF>

class QWidget;

export module Artifact.Widgets.LayerEditor.ContextMenu;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;

export namespace Artifact {

enum class LayerEditorBackgroundMode { Alpha, Solid, MayaGradient };

enum class LayerEditorShapeContextCommand {
 None,
 ClearOperators,
 AddTrimPaths,
 AddRepeater,
 AddMergePaths,
 AddOffsetPaths,
 AddPuckerBloat,
 AddRoundedCorners,
 AddWigglePaths,
 AddZigZag,
 AddTwist,
 AddHandDrawnWobble,
 RemoveOperator,
 MoveOperatorUp,
 MoveOperatorDown,
 InsertPolygonPoint,
 SplitPolygonSegment,
 DeletePolygonPoint,
 TogglePolygonClosed,
 ConvertToPath,
 ConvertToPolygon,
 DeletePathPoint,
 TogglePathSmooth,
 TogglePathClosed
};

struct LayerEditorShapeContextChoice {
 LayerEditorShapeContextCommand command = LayerEditorShapeContextCommand::None;
 int operatorIndex = -1;
};

enum class LayerEditorShapeContextResultTarget { None, Polygon, Path };

struct LayerEditorShapeContextApplyResult {
 bool handled = false;
 LayerEditorShapeContextResultTarget hoverTarget =
     LayerEditorShapeContextResultTarget::None;
 int hoveredVertex = -1;
 int hoveredSegment = -1;
};

enum class LayerEditorBackgroundContextCommand {
 None, ToggleGrid, ToggleSafeMargins, Alpha, Solid, MayaGradient
};

struct LayerEditorContextMenuRunResult {
 bool consumed = false;
 bool changed = false;
};

LayerEditorShapeContextChoice showLayerEditorShapeContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    const ArtifactShapeLayer& shape,
    int hoveredPolygonVertex, int hoveredPolygonSegment,
    int hoveredPathVertex);

LayerEditorShapeContextApplyResult applyLayerEditorShapeContextCommand(
    const ArtifactAbstractLayerPtr& layer, ArtifactShapeLayer& shape,
    const LayerEditorShapeContextChoice& choice,
    const QPointF& canvasPosition,
    int hoveredPolygonVertex, int hoveredPolygonSegment,
    int hoveredPathVertex,
    LayerEditorShapeEditSession& editSession);

LayerEditorBackgroundContextCommand showLayerEditorBackgroundContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    bool showGrid, bool showSafeMargins,
    LayerEditorBackgroundMode backgroundMode);

LayerEditorContextMenuRunResult runLayerEditorShapeContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    const QPointF& viewportPosition, ArtifactIRenderer* renderer,
    const ArtifactAbstractLayerPtr& layer,
    LayerEditorShapeHoverController& hoverController,
    LayerEditorShapeEditSession& editSession);

bool runLayerEditorBackgroundContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    bool& showGrid, bool& showSafeMargins,
    LayerEditorBackgroundMode& backgroundMode);

}
