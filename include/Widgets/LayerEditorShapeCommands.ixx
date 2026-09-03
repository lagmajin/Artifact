module;

#include <QJsonArray>
#include <QPointF>

#include <memory>
#include <vector>

export module Artifact.Widgets.LayerEditor.ShapeCommands;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Undo.UndoManager;

export namespace Artifact {

std::unique_ptr<UndoCommand> makeShapeEditCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<QPointF> beforePoints,
    std::vector<QPointF> afterPoints,
    bool beforeClosed, bool afterClosed);

std::unique_ptr<UndoCommand> makeCornerRadiusEditCommand(
    const ArtifactAbstractLayerPtr& layer, float before, float after);

std::unique_ptr<UndoCommand> makeStarInnerRadiusEditCommand(
    const ArtifactAbstractLayerPtr& layer, float before, float after);

std::unique_ptr<UndoCommand> makePathVertexEditCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<CustomPathVertex> before,
    std::vector<CustomPathVertex> after,
    bool beforeClosed, bool afterClosed);

std::unique_ptr<UndoCommand> makeShapeConversionCommand(
    const ArtifactAbstractLayerPtr& layer,
    std::vector<QPointF> beforePolygon, bool beforePolygonClosed,
    std::vector<CustomPathVertex> beforePath, bool beforePathClosed,
    std::vector<QPointF> afterPolygon, bool afterPolygonClosed,
    std::vector<CustomPathVertex> afterPath, bool afterPathClosed);

std::unique_ptr<UndoCommand> makeShapeOperatorStackCommand(
    const ArtifactAbstractLayerPtr& layer,
    QJsonArray before, QJsonArray after);

}
