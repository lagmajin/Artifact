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
	
  class ArtifactIRenderer
  {
  private:
   class Impl;
   Impl* impl_;
  public:
   explicit ArtifactIRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext,QWidget*widget);
   ArtifactIRenderer();
   virtual ~ArtifactIRenderer();
   void initialize(QWidget* widget);
   void createSwapChain(QWidget* widget);
   void recreateSwapChain(QWidget* widget);
   virtual void drawSprite(float x,float y,float w,float h);
   virtual void drawSprite(float2 pos, float2 size);
   virtual void drawSolidRect(float x, float y, float w, float h);
   virtual void drawSolidRect(float2 pos, float2 size,const FloatColor& color);
   virtual void drawRectOutline(float x, float y, float w, float h, const FloatColor& color);
   virtual void drawRectOutline(float2 pos, float2 size, const FloatColor& color);
   virtual void drawParticles();
   virtual void drawRectLocal(float x, float y, float w, float h, const FloatColor& color);
   virtual void drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color);
   virtual void drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color);
   virtual void drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color); // Quadratic
   virtual void drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color); // Cubic
   virtual void drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color);
 
   // Viewport Transform (Proposal 4)
   virtual void setViewportSize(float w, float h);
   virtual void setCanvasSize(float w, float h);
    virtual void setPan(float x, float y);
    virtual void setZoom(float zoom);
    virtual void panBy(float dx, float dy);
    virtual void resetView();
    virtual void fitToViewport(float margin = 50.0f);
    virtual void zoomAroundViewportPoint(float2 viewportPos, float newZoom);
 
    virtual float2 canvasToViewport(float2 canvasPos) const;
   virtual float2 viewportToCanvas(float2 pos) const;
 
  virtual void setUpscaleConfig(bool enable, float sharpness = 0.5f);
 
   
  void clear();
  void flush();
  void present();
  void flushAndWait();
  void beginFrameGpuProfiling();
  void endFrameGpuProfiling();
  double lastFrameGpuTimeMs() const;
  void destroy();

  public/*signals*/:
   
 };












};
