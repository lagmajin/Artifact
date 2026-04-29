module;
#include <utility>

module Artifact.Layer.AdjustableLayer;

import Size;
import Artifact.Layers.Abstract._2D;
import Artifact.Composition.Abstract;

namespace Artifact
{

ArtifactAdjustableLayer::ArtifactAdjustableLayer()
{
    setLayerName("Adjustment Layer");
    setAdjustmentLayer(true);
}

ArtifactAdjustableLayer::~ArtifactAdjustableLayer()
{
}

void ArtifactAdjustableLayer::setComposition(void *comp)
{
    ArtifactAbstractLayer::setComposition(comp);
    if (auto *composition = static_cast<ArtifactAbstractComposition*>(comp)) {
        const auto compSize = composition->settings().compositionSize();
        if (compSize.width() > 0 && compSize.height() > 0) {
            setSourceSize(Size_2D{compSize.width(), compSize.height()});
        }
    }
}

void ArtifactAdjustableLayer::draw(ArtifactIRenderer* renderer)
{
    (void)renderer;
    // Adjustment layers are effect carriers. They do not draw their own pixels.
    // Their effects are applied by the composition renderer when appropriate.
}

bool ArtifactAdjustableLayer::isAdjustmentLayer() const
{
    return true;
}

bool ArtifactAdjustableLayer::isNullLayer() const
{
    return false;
}

}
