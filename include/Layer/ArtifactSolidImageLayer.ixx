module;


export module Artifact.Layers.SolidImage;

import Color.Float;


export namespace Artifact
{
 using namespace ArtifactCore;

 class ArtifactSolidImageLayerSettings
 {
 private:

 public:
  ArtifactSolidImageLayerSettings();
  ~ArtifactSolidImageLayerSettings();
 };


 class ArtifactSolidImageLayer
 {private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactSolidImageLayer();
  ~ArtifactSolidImageLayer();
  FloatColor layerColor() const;
  void setLayerColor(const FloatColor& color);
 };





}