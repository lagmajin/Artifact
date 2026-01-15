module;
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <QWidget>

export module Artifact.Render.IRenderer;

import std;
import Color.Float;
import Utils.Size.Like;
import Utils.Point.Like;


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
  explicit IRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext,QWidget*widget);
  virtual ~IRenderer();
  void recreateSwapChain(QWidget* widget);
  virtual void drawSprite(float x,float y,float w,float h);
  virtual void drawSprite(float2 pos, float2 size);
  virtual void drawSolidRect(float x, float y,float w,float h);
  virtual void drawSolidRect(float2 pos, float2 size,const FloatColor& color);
  virtual void drawRectOutline();
  virtual void drawParticles();
  
  virtual void setUpscaleConfig(bool enable, float sharpness = 0.5f);
 
   
  void clear();
  void flush();
  void flushAndWait();
   
  public/*signals*/:
   
 };












};