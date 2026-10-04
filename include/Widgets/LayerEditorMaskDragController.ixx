module;

#include <QPointF>

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskDragController;

import Artifact.Layer.Abstract;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.Geometry;

export namespace Artifact {

class LayerEditorMaskDragController {
public:
 bool dragHandle(const ArtifactAbstractLayerPtr& layer,
                 const QPointF& canvasPosition,
                 int maskIndex, int pathIndex, int vertexIndex,
                 MaskHandleType handleType) const;

bool dragVertex(const ArtifactAbstractLayerPtr& layer,
                 const QPointF& canvasPosition,
                 int maskIndex, int pathIndex, int vertexIndex,
                 bool proportionalDragActive,
                 const std::vector<MaskVertex>& proportionalBefore,
                 const QPointF& proportionalOrigin,
                 float proportionalRadius) const;

 /// 指定マスク（maskIndex < 0 なら全マスク）の全パスの頂点に
 /// アフィン変換を適用してベイクする。feather / expansion も倍率に応じて
 /// 追従する（AE 風）。undo は呼び出し側の EditSession が担う。
 bool transformMasks(const ArtifactAbstractLayerPtr& layer,
                     const MaskAffineTransform& transform,
                     int maskIndex) const;
};

}
