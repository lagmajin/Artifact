module;
#include <vector>
#include <QJsonArray>

export module Artifact.Layer.Serialization;

import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;

export namespace Artifact {

// JSON schema ownership for layer-adjacent value objects. This module never
// imports Artifact.Layer.Abstract, keeping Impl private to its owning unit.
QJsonArray serializeLayerMasks(const std::vector<LayerMask>& masks);
std::vector<LayerMask> deserializeLayerMasks(const QJsonArray& masks);

} // namespace Artifact
