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
	
  class AritfactIRenderer
  {
  private:
   class Impl;
   Impl* impl_;
  public:
   explicit AritfactIRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext,QWidget*widget);
   AritfactIRenderer();
   virtual ~AritfactIRenderer();
   void initialize(QWidget* widget);
   void createSwapChain(QWidget* widget);
   void recreateSwapChain(QWidget* widget);
   virtual void drawSprite(float x,float y,float w,float h);
   virtual void drawSprite(float2 pos, float2 size);
   virtual void drawSolidRect(float x, float y,float w,float h);
   virtual void drawSolidRect(float2 pos, float2 size,const FloatColor& color);
   virtual void drawRectOutline();
   virtual void drawParticles();
   virtual void drawRectLocal(float x, float y, float w, float h, const FloatColor& color);
  
  virtual void setUpscaleConfig(bool enable, float sharpness = 0.5f);
 
   
  void clear();
  void flush();
  void present();
  void flushAndWait();
  void destroy();

  public/*signals*/:
   
 };












};