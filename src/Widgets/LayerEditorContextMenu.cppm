module;

#include <QAction>
#include <QJsonArray>
#include <QMenu>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QTransform>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <utility>
#include <vector>

module Artifact.Widgets.LayerEditor.ContextMenu;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Render.IRenderer;
import Artifact.Widgets.LayerEditor.ShapeCommands;
import Artifact.Widgets.LayerEditor.ShapeEditSession;
import Artifact.Widgets.LayerEditor.ShapeHoverController;
import Memory.SharedPtr;
import Settings.Accessibility;
import Shape.Operator;
import Undo.UndoManager;

namespace Artifact {
namespace {

QString operatorTypeName(ArtifactCore::ShapeOperatorType type)
{
 switch (type) {
  case ArtifactCore::ShapeOperatorType::TrimPaths: return QStringLiteral("Trim Paths");
  case ArtifactCore::ShapeOperatorType::Repeater: return QStringLiteral("Repeater");
  case ArtifactCore::ShapeOperatorType::MergePaths: return QStringLiteral("Merge Paths");
  case ArtifactCore::ShapeOperatorType::OffsetPaths: return QStringLiteral("Offset Paths");
  case ArtifactCore::ShapeOperatorType::PuckerBloat: return QStringLiteral("Pucker & Bloat");
  case ArtifactCore::ShapeOperatorType::RoundedCorners: return QStringLiteral("Rounded Corners");
  case ArtifactCore::ShapeOperatorType::WigglePaths: return QStringLiteral("Wiggle Paths");
  case ArtifactCore::ShapeOperatorType::ZigZag: return QStringLiteral("Zig Zag");
  case ArtifactCore::ShapeOperatorType::Twist: return QStringLiteral("Twist");
  case ArtifactCore::ShapeOperatorType::HandDrawnWobble: return QStringLiteral("Hand Drawn Wobble");
  default: return QStringLiteral("Unknown Operator");
 }
}

struct ActionChoice {
 QAction* action = nullptr;
 LayerEditorShapeContextChoice choice;
};

void addChoice(QMenu& menu, std::vector<ActionChoice>& choices,
               const QString& label, LayerEditorShapeContextCommand command,
               bool enabled = true, int operatorIndex = -1)
{
 auto* action = menu.addAction(label);
 action->setEnabled(enabled);
 choices.push_back({action, {command, operatorIndex}});
}

void addChoice(QMenu& menu, std::vector<ActionChoice>& choices,
               QAction* action, LayerEditorShapeContextCommand command,
               int operatorIndex = -1)
{
 choices.push_back({action, {command, operatorIndex}});
}

QAction* executeMenu(QMenu& menu, const QPoint& globalPosition)
{
 int menuX = globalPosition.x();
 int menuY = globalPosition.y();
 Accessibility::adjustContextMenuPosition(menuX, menuY, menu.sizeHint().width());
 return menu.exec(QPoint(menuX, menuY));
}

}

LayerEditorShapeContextChoice showLayerEditorShapeContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    const ArtifactShapeLayer& shape,
    int hoveredPolygonVertex, int hoveredPolygonSegment,
    int hoveredPathVertex)
{
 QMenu menu(parent);
 std::vector<ActionChoice> choices;
 choices.reserve(32);

 auto* addOperatorMenu = menu.addMenu(QStringLiteral("Add Operator"));
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Trim Paths")),
           LayerEditorShapeContextCommand::AddTrimPaths);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Repeater")),
           LayerEditorShapeContextCommand::AddRepeater);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Merge Paths")),
           LayerEditorShapeContextCommand::AddMergePaths);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Offset Paths")),
           LayerEditorShapeContextCommand::AddOffsetPaths);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Pucker & Bloat")),
           LayerEditorShapeContextCommand::AddPuckerBloat);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Rounded Corners")),
           LayerEditorShapeContextCommand::AddRoundedCorners);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Wiggle Paths")),
           LayerEditorShapeContextCommand::AddWigglePaths);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("ZigZag")),
           LayerEditorShapeContextCommand::AddZigZag);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Twist")),
           LayerEditorShapeContextCommand::AddTwist);
 addChoice(*addOperatorMenu, choices,
           addOperatorMenu->addAction(QStringLiteral("Hand Drawn Wobble")),
           LayerEditorShapeContextCommand::AddHandDrawnWobble);

 addChoice(menu, choices, QStringLiteral("Clear Shape Operators"),
           LayerEditorShapeContextCommand::ClearOperators,
           shape.shapeOperatorCount() > 0);
 if (shape.shapeOperatorCount() > 0) {
  auto* manageMenu = menu.addMenu(QStringLiteral("Manage Operators"));
  for (int index = 0; index < shape.shapeOperatorCount(); ++index) {
   auto* itemMenu = manageMenu->addMenu(
       QString::number(index + 1) + QStringLiteral(". ") +
       operatorTypeName(shape.shapeOperatorTypeAt(index)));
   addChoice(*itemMenu, choices, itemMenu->addAction(QStringLiteral("Remove")),
             LayerEditorShapeContextCommand::RemoveOperator, index);
   if (index > 0)
    addChoice(*itemMenu, choices, itemMenu->addAction(QStringLiteral("Move Up")),
              LayerEditorShapeContextCommand::MoveOperatorUp, index);
   if (index < shape.shapeOperatorCount() - 1)
    addChoice(*itemMenu, choices, itemMenu->addAction(QStringLiteral("Move Down")),
              LayerEditorShapeContextCommand::MoveOperatorDown, index);
  }
 }

 if (shape.hasCustomPolygon()) {
  addChoice(menu, choices, QStringLiteral("Insert Point"),
            LayerEditorShapeContextCommand::InsertPolygonPoint,
            hoveredPolygonSegment >= 0);
  addChoice(menu, choices, QStringLiteral("Split Segment"),
            LayerEditorShapeContextCommand::SplitPolygonSegment,
            hoveredPolygonSegment >= 0);
  addChoice(menu, choices, QStringLiteral("Delete Point"),
            LayerEditorShapeContextCommand::DeletePolygonPoint,
            hoveredPolygonVertex >= 0);
  addChoice(menu, choices,
            shape.customPolygonClosed() ? QStringLiteral("Open Polygon")
                                        : QStringLiteral("Close Polygon"),
            LayerEditorShapeContextCommand::TogglePolygonClosed,
            shape.customPolygonClosed() || shape.customPolygonPoints().size() >= 3);
  addChoice(menu, choices, QStringLiteral("Convert to Editable Path"),
            LayerEditorShapeContextCommand::ConvertToPath);
 }
 if (shape.hasCustomPath()) {
  const auto vertices = shape.customPathVertices();
  const bool validHover = hoveredPathVertex >= 0 &&
      hoveredPathVertex < static_cast<int>(vertices.size());
  addChoice(menu, choices, QStringLiteral("Convert to Polygon"),
            LayerEditorShapeContextCommand::ConvertToPolygon);
  addChoice(menu, choices, QStringLiteral("Delete Point"),
            LayerEditorShapeContextCommand::DeletePathPoint, validHover);
  addChoice(menu, choices,
            validHover && vertices[static_cast<size_t>(hoveredPathVertex)].smooth
                ? QStringLiteral("Make Corner") : QStringLiteral("Make Smooth"),
            LayerEditorShapeContextCommand::TogglePathSmooth, validHover);
  addChoice(menu, choices,
            shape.customPathClosed() ? QStringLiteral("Open Path")
                                     : QStringLiteral("Close Path"),
            LayerEditorShapeContextCommand::TogglePathClosed);
 }

 auto* chosen = executeMenu(menu, globalPosition);
 for (const auto& candidate : choices)
  if (candidate.action == chosen) return candidate.choice;
 return {};
}

bool applyShapeOperatorCommand(
    const ArtifactAbstractLayerPtr& layer, ArtifactShapeLayer& shape,
    const LayerEditorShapeContextChoice& choice)
{
 const QJsonArray before =
     shape.toJson().value(QStringLiteral("shapeOperators")).toArray();
 bool handled = true;
 switch (choice.command) {
  case LayerEditorShapeContextCommand::ClearOperators:
   shape.clearShapeOperators();
   break;
  case LayerEditorShapeContextCommand::AddTrimPaths:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::TrimPaths);
   break;
  case LayerEditorShapeContextCommand::AddRepeater:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::Repeater);
   break;
  case LayerEditorShapeContextCommand::AddMergePaths:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::MergePaths);
   break;
  case LayerEditorShapeContextCommand::AddOffsetPaths:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::OffsetPaths);
   break;
  case LayerEditorShapeContextCommand::AddPuckerBloat:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::PuckerBloat);
   break;
  case LayerEditorShapeContextCommand::AddRoundedCorners:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::RoundedCorners);
   break;
  case LayerEditorShapeContextCommand::AddWigglePaths:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::WigglePaths);
   break;
  case LayerEditorShapeContextCommand::AddZigZag:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::ZigZag);
   break;
  case LayerEditorShapeContextCommand::AddTwist:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::Twist);
   break;
  case LayerEditorShapeContextCommand::AddHandDrawnWobble:
   shape.addShapeOperator(ArtifactCore::ShapeOperatorType::HandDrawnWobble);
   break;
  case LayerEditorShapeContextCommand::RemoveOperator:
   if (choice.operatorIndex >= 0 && choice.operatorIndex < shape.shapeOperatorCount())
    shape.removeShapeOperatorAt(choice.operatorIndex);
   else handled = false;
   break;
  case LayerEditorShapeContextCommand::MoveOperatorUp:
   if (choice.operatorIndex > 0 && choice.operatorIndex < shape.shapeOperatorCount())
    shape.moveShapeOperator(choice.operatorIndex, choice.operatorIndex - 1);
   else handled = false;
   break;
  case LayerEditorShapeContextCommand::MoveOperatorDown:
   if (choice.operatorIndex >= 0 &&
       choice.operatorIndex < shape.shapeOperatorCount() - 1)
    shape.moveShapeOperator(choice.operatorIndex, choice.operatorIndex + 1);
   else handled = false;
   break;
  default:
   handled = false;
   break;
 }
 if (!handled) return false;
 const QJsonArray after =
     shape.toJson().value(QStringLiteral("shapeOperators")).toArray();
 if (before == after) return true;
 if (auto* undo = UndoManager::instance();
     undo && !undo->push(makeShapeOperatorStackCommand(layer, before, after))) {
  shape.restoreOperatorsFromJson(before);
 }
 return true;
}

LayerEditorShapeContextApplyResult applyLayerEditorShapeContextCommand(
    const ArtifactAbstractLayerPtr& layer, ArtifactShapeLayer& shape,
    const LayerEditorShapeContextChoice& choice,
    const QPointF& canvasPosition,
    int hoveredPolygonVertex, int hoveredPolygonSegment,
    int hoveredPathVertex,
    LayerEditorShapeEditSession& editSession)
{
 LayerEditorShapeContextApplyResult result;
 if (!layer) return result;
 if (applyShapeOperatorCommand(layer, shape, choice)) {
  result.handled = true;
  return result;
 }

 if (shape.hasCustomPolygon()) {
  auto points = shape.customPolygonPoints();
  const bool closed = shape.customPolygonClosed();
  if (choice.command == LayerEditorShapeContextCommand::DeletePolygonPoint) {
   if (hoveredPolygonVertex < 0 ||
       hoveredPolygonVertex >= static_cast<int>(points.size())) return result;
   editSession.beginPolygon(layer);
   points.erase(points.begin() + hoveredPolygonVertex);
   if (points.size() >= 3) shape.setCustomPolygonPoints(points, closed);
   else shape.clearCustomPolygonPoints();
   editSession.markPolygonDirty();
   editSession.commitPolygon();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Polygon;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::InsertPolygonPoint) {
   if (hoveredPolygonSegment < 0 || points.size() < 2) return result;
   const int count = closed ? static_cast<int>(points.size())
                            : static_cast<int>(points.size()) - 1;
   if (count <= 0) return result;
   bool invertible = false;
   const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
   if (!invertible) return result;
   const QPointF raw = inverse.map(canvasPosition);
   const QPointF local(
       std::clamp(raw.x(), 0.0, static_cast<double>(shape.shapeWidth())),
       std::clamp(raw.y(), 0.0, static_cast<double>(shape.shapeHeight())));
   const int segment = std::clamp(hoveredPolygonSegment, 0, count - 1);
   const int inserted = std::clamp(segment + 1, 0, static_cast<int>(points.size()));
   editSession.beginPolygon(layer);
   points.insert(points.begin() + inserted, local);
   shape.setCustomPolygonPoints(points, closed);
   editSession.markPolygonDirty();
   editSession.commitPolygon();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Polygon;
   result.hoveredVertex = inserted;
   result.hoveredSegment = inserted - 1;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::SplitPolygonSegment) {
   if (hoveredPolygonSegment < 0 || points.size() < 2) return result;
   const int count = closed ? static_cast<int>(points.size())
                            : static_cast<int>(points.size()) - 1;
   if (count <= 0) return result;
   const int segment = std::clamp(hoveredPolygonSegment, 0, count - 1);
   const int next = closed ? (segment + 1) % static_cast<int>(points.size())
                           : segment + 1;
   if (next < 0 || next >= static_cast<int>(points.size())) return result;
   const QPointF point = (points[static_cast<size_t>(segment)] +
                          points[static_cast<size_t>(next)]) * 0.5;
   const int inserted = std::clamp(segment + 1, 0, static_cast<int>(points.size()));
   editSession.beginPolygon(layer);
   points.insert(points.begin() + inserted, point);
   shape.setCustomPolygonPoints(points, closed);
   editSession.markPolygonDirty();
   editSession.commitPolygon();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Polygon;
   result.hoveredVertex = inserted;
   result.hoveredSegment = inserted - 1;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::TogglePolygonClosed) {
   if (!closed && points.size() < 3) return result;
   editSession.beginPolygon(layer);
   shape.setCustomPolygonPoints(points, !closed);
   editSession.markPolygonDirty();
   editSession.commitPolygon();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Polygon;
   result.hoveredVertex = hoveredPolygonVertex;
   result.hoveredSegment = hoveredPolygonSegment;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::ConvertToPath) {
   const auto beforePath = shape.customPathVertices();
   const bool beforePathClosed = shape.customPathClosed();
   std::vector<CustomPathVertex> vertices;
   vertices.reserve(points.size());
   for (const auto& point : points)
    vertices.push_back({point, QPointF(), QPointF(), false});
   shape.setCustomPathVertices(vertices, closed);
   shape.clearCustomPolygonPoints();
   auto* undo = UndoManager::instance();
   bool converted = true;
   if (undo && !undo->push(makeShapeConversionCommand(
           layer, points, closed, beforePath, beforePathClosed,
           std::vector<QPointF>{}, false, vertices, closed))) {
    shape.setCustomPolygonPoints(points, closed);
    if (beforePath.empty()) shape.clearCustomPath();
    else shape.setCustomPathVertices(beforePath, beforePathClosed);
    shape.changed();
    converted = false;
   }
   result.handled = true;
   result.hoverTarget = converted ? LayerEditorShapeContextResultTarget::Path
                                  : LayerEditorShapeContextResultTarget::Polygon;
   result.hoveredVertex = converted ? (vertices.empty() ? -1 : 0)
                                    : hoveredPolygonVertex;
   result.hoveredSegment = converted ? -1 : hoveredPolygonSegment;
   return result;
  }
 }

 if (shape.hasCustomPath()) {
  auto vertices = shape.customPathVertices();
  const bool closed = shape.customPathClosed();
  if (choice.command == LayerEditorShapeContextCommand::DeletePathPoint) {
   if (hoveredPathVertex < 0 ||
       hoveredPathVertex >= static_cast<int>(vertices.size())) return result;
   editSession.beginPath(layer);
   vertices.erase(vertices.begin() + hoveredPathVertex);
   if (vertices.size() >= 3) shape.setCustomPathVertices(vertices, closed);
   else shape.clearCustomPath();
   editSession.markPathDirty();
   editSession.commitPath();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Path;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::TogglePathSmooth) {
   if (hoveredPathVertex < 0 ||
       hoveredPathVertex >= static_cast<int>(vertices.size())) return result;
   editSession.beginPath(layer);
   auto& vertex = vertices[static_cast<size_t>(hoveredPathVertex)];
   vertex.smooth = !vertex.smooth;
   shape.setCustomPathVertices(vertices, closed);
   editSession.markPathDirty();
   editSession.commitPath();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Path;
   result.hoveredVertex = hoveredPathVertex;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::TogglePathClosed) {
   if (!closed && vertices.size() < 3) return result;
   editSession.beginPath(layer);
   shape.setCustomPathVertices(vertices, !closed);
   editSession.markPathDirty();
   editSession.commitPath();
   result.handled = true;
   result.hoverTarget = LayerEditorShapeContextResultTarget::Path;
   result.hoveredVertex = hoveredPathVertex;
   return result;
  }
  if (choice.command == LayerEditorShapeContextCommand::ConvertToPolygon) {
   const auto beforePolygon = shape.customPolygonPoints();
   const bool beforePolygonClosed = shape.customPolygonClosed();
   std::vector<QPointF> points;
   points.reserve(vertices.size());
   for (const auto& vertex : vertices) points.push_back(vertex.pos);
   shape.setCustomPolygonPoints(points, closed);
   shape.clearCustomPath();
   auto* undo = UndoManager::instance();
   bool converted = true;
   if (undo && !undo->push(makeShapeConversionCommand(
           layer, beforePolygon, beforePolygonClosed, vertices, closed,
           points, closed, std::vector<CustomPathVertex>{}, false))) {
    if (beforePolygon.empty()) shape.clearCustomPolygonPoints();
    else shape.setCustomPolygonPoints(beforePolygon, beforePolygonClosed);
    shape.setCustomPathVertices(vertices, closed);
    shape.changed();
    converted = false;
   }
   result.handled = true;
   result.hoverTarget = converted ? LayerEditorShapeContextResultTarget::Polygon
                                  : LayerEditorShapeContextResultTarget::Path;
   result.hoveredVertex = converted ? (points.empty() ? -1 : 0)
                                    : hoveredPathVertex;
   result.hoveredSegment = converted && points.size() >= 2 ? 0 : -1;
   return result;
  }
 }
 return result;
}

LayerEditorBackgroundContextCommand showLayerEditorBackgroundContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    bool showGrid, bool showSafeMargins,
    LayerEditorBackgroundMode backgroundMode)
{
 QMenu menu(parent);
 auto* grid = menu.addAction(QStringLiteral("Show Composition Grid"));
 grid->setCheckable(true);
 grid->setChecked(showGrid);
 auto* safe = menu.addAction(QStringLiteral("Show Safe Margins"));
 safe->setCheckable(true);
 safe->setChecked(showSafeMargins);
 menu.addSeparator();
 auto* alpha = menu.addAction(QStringLiteral("Alpha"));
 auto* solid = menu.addAction(QStringLiteral("Solid"));
 auto* maya = menu.addAction(QStringLiteral("Maya Gradient"));
 for (auto* action : {alpha, solid, maya}) action->setCheckable(true);
 if (backgroundMode == LayerEditorBackgroundMode::Alpha) alpha->setChecked(true);
 if (backgroundMode == LayerEditorBackgroundMode::Solid) solid->setChecked(true);
 if (backgroundMode == LayerEditorBackgroundMode::MayaGradient) maya->setChecked(true);

 auto* chosen = executeMenu(menu, globalPosition);
 if (chosen == grid) return LayerEditorBackgroundContextCommand::ToggleGrid;
 if (chosen == safe) return LayerEditorBackgroundContextCommand::ToggleSafeMargins;
 if (chosen == alpha) return LayerEditorBackgroundContextCommand::Alpha;
 if (chosen == solid) return LayerEditorBackgroundContextCommand::Solid;
 if (chosen == maya) return LayerEditorBackgroundContextCommand::MayaGradient;
 return LayerEditorBackgroundContextCommand::None;
}

LayerEditorContextMenuRunResult runLayerEditorShapeContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    const QPointF& viewportPosition, ArtifactIRenderer* renderer,
    const ArtifactAbstractLayerPtr& layer,
    LayerEditorShapeHoverController& hoverController,
    LayerEditorShapeEditSession& editSession)
{
 auto shape = layer && layer->isVisible() && !layer->isLocked()
     ? ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
           ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer))
     : ArtifactCore::SharedPtr<ArtifactShapeLayer>{};
 if (!shape || !renderer) return {};
 const auto canvas = renderer->viewportToCanvas(
     {static_cast<float>(viewportPosition.x()),
      static_cast<float>(viewportPosition.y())});
 const QPointF canvasPosition(canvas.x, canvas.y);
 if (shape->hasCustomPolygon())
  hoverController.updatePolygon(layer, canvasPosition, renderer->getZoom());

 const auto hover = hoverController.state();
 const auto choice = showLayerEditorShapeContextMenu(
     parent, globalPosition, *shape,
     hover.polygonVertex, hover.polygonSegment, hover.pathVertex);
 if (choice.command == LayerEditorShapeContextCommand::None)
  return {true, false};
 const auto applied = applyLayerEditorShapeContextCommand(
     layer, *shape, choice, canvasPosition,
     hover.polygonVertex, hover.polygonSegment, hover.pathVertex, editSession);
 if (applied.hoverTarget == LayerEditorShapeContextResultTarget::Polygon)
  hoverController.setPolygon(applied.hoveredVertex, applied.hoveredSegment);
 else if (applied.hoverTarget == LayerEditorShapeContextResultTarget::Path)
  hoverController.setPath(applied.hoveredVertex, -1, 0);
 return {true, applied.handled};
}

bool runLayerEditorBackgroundContextMenu(
    QWidget* parent, const QPoint& globalPosition,
    bool& showGrid, bool& showSafeMargins,
    LayerEditorBackgroundMode& backgroundMode)
{
 const auto command = showLayerEditorBackgroundContextMenu(
     parent, globalPosition, showGrid, showSafeMargins, backgroundMode);
 switch (command) {
  case LayerEditorBackgroundContextCommand::ToggleGrid:
   showGrid = !showGrid;
   return true;
  case LayerEditorBackgroundContextCommand::ToggleSafeMargins:
   showSafeMargins = !showSafeMargins;
   return true;
  case LayerEditorBackgroundContextCommand::Alpha:
   backgroundMode = LayerEditorBackgroundMode::Alpha;
   return true;
  case LayerEditorBackgroundContextCommand::Solid:
   backgroundMode = LayerEditorBackgroundMode::Solid;
   return true;
  case LayerEditorBackgroundContextCommand::MayaGradient:
   backgroundMode = LayerEditorBackgroundMode::MayaGradient;
   return true;
  case LayerEditorBackgroundContextCommand::None:
   return false;
 }
 return false;
}

}
