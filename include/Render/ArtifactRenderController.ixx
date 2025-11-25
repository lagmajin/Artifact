module;
export module Artifact.Render.Controller;

export namespace ArtifactCore
{
 //using namespace Diligent;


 class ArtifactRenderController
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactRenderController();
  ~ArtifactRenderController();
  void drawRect();
  void drawRectOutline();
  void drawSprite();
 };

 class ArtifactLayerRenderContext {
 private:
  class Impl;
  Impl* impl_;
 public:

 };

 class AartifactCompositionRenderContext {
 private:
  class Impl;
  Impl* impl_;
 public:
  AartifactCompositionRenderContext();
  ~AartifactCompositionRenderContext();
 };









};