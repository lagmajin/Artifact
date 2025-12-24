module;
#include <DiligentCore/Common/interface/BasicMath.hpp>



export module Artifact.Render.IRenderer;

import std;
import Color.Float;


export namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;
	
 class IRenderer
 {
 private:
  class Impl;
  Impl* impl_;
 public:
  IRenderer();
  explicit IRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, RefCntAutoPtr<ISwapChain> pSwapChain);
  ~IRenderer();
  virtual void drawSprite(float x,float y,float w,float h);
  virtual void drawSprite(float2 pos, float2 size);
  virtual void drawSolidRect(float x, float y,float w,float h);
  virtual void drawSolidRect(float2 pos, float2 size,const FloatColor& color);
  virtual void drawRectOutline();
  virtual void drawParticles();
 };












};