module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.Geometry;

import Artifact.Layer.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.Path;

export namespace Artifact {

enum class MaskHandleType { None, InTangent, OutTangent };

QPointF maskHandlePosition(const MaskPath& path, int vertexIndex,
                           MaskHandleType handleType);

float proportionalEditWeight(qreal distance, qreal radius);

std::vector<MaskVertex> proportionalMaskVertices(
    const std::vector<MaskVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius);
std::vector<QPointF> proportionalShapePoints(
    const std::vector<QPointF>& source, const QPointF& origin,
    const QPointF& target, qreal radius, qreal width, qreal height);
std::vector<CustomPathVertex> proportionalPathVertices(
    const std::vector<CustomPathVertex>& source, const QPointF& origin,
    const QPointF& target, qreal radius);

std::vector<QPointF> buildShapeEditSeedPoints(
    const ArtifactShapeLayer& shape);
bool ensureShapeEditSeedGeometry(const ArtifactAbstractLayerPtr& layer);

QPointF shapeCornerRadiusHandlePosition(const ArtifactShapeLayer& shape);
QPointF shapeStarInnerRadiusHandlePosition(const ArtifactShapeLayer& shape);

int extrudePolygonVertex(std::vector<QPointF>& points, int sourceIndex);
int extrudePathVertex(std::vector<CustomPathVertex>& vertices, int sourceIndex);

std::vector<QPointF> translateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& delta);
std::vector<CustomPathVertex> translateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& delta);
std::vector<CustomPathVertex> rotateSelectedPath(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, double radians);
std::vector<QPointF> rotateSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double radians);
std::vector<QPointF> scaleSelectedPolygon(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    double factor);
std::vector<QPointF> scaleSelectedPolygonAxes(
    const std::vector<QPointF>& source, const std::vector<int>& selected,
    const QPointF& factors);
std::vector<CustomPathVertex> scaleSelectedPathAxes(
    const std::vector<CustomPathVertex>& source,
    const std::vector<int>& selected, const QPointF& factors);
std::vector<QPointF> insetPolygon(
    const std::vector<QPointF>& source, double factor);

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPos, float threshold,
                       int& maskIndex, int& pathIndex, int& vertexIndex,
                       MaskHandleType& handleType);

bool hitTestMaskVertexGeometry(const ArtifactAbstractLayerPtr& layer,
                               const QPointF& canvasPos, float threshold,
                               int& maskIndex, int& pathIndex,
                               int& vertexIndex);

}
