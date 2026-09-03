module;

#include <vector>

export module Artifact.Widgets.LayerEditor.MaskEditSession;

import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;

export namespace Artifact {

class LayerEditorMaskEditSession {
public:
 void begin(const ArtifactAbstractLayerPtr& layer);
 void markDirty();
 void commit();
 void cancel();
 bool pending() const noexcept;

private:
 bool pending_ = false;
 bool dirty_ = false;
 ArtifactAbstractLayerWeak layer_;
 std::vector<LayerMask> before_;
};

}
