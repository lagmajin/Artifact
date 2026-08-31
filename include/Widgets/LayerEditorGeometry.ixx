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

std::vector<QPointF> buildShapeEditSeedPoints(
    const ArtifactShapeLayer& shape);

bool hitTestMaskHandle(const ArtifactAbstractLayerPtr& layer,
                       const QPointF& canvasPos, float threshold,
                       int& maskIndex, int& pathIndex, int& vertexIndex,
                       MaskHandleType& handleType);

}
