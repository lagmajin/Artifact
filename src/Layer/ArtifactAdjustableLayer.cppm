module Artifact.Layer.AdjustableLayer;

import std;

namespace Artifact
{

 ArtifactAdjustableLayer::ArtifactAdjustableLayer()
 {

 }

 ArtifactAdjustableLayer::~ArtifactAdjustableLayer()
 {

 }

 void ArtifactAdjustableLayer::draw()
 {
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
