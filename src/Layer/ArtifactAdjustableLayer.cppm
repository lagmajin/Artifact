module;
#include <utility>

#include <stdexcept>

module Artifact.Layer.AdjustableLayer;

namespace Artifact
{

ArtifactAdjustableLayer::ArtifactAdjustableLayer()
{
}

ArtifactAdjustableLayer::~ArtifactAdjustableLayer()
{
}

void ArtifactAdjustableLayer::draw(ArtifactIRenderer* renderer)
{
    (void)renderer;
    throw std::logic_error("The method or operation is not implemented.");
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
