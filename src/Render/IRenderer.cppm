module;
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>


module Artifact.Render.IRenderer;

import std;


namespace Artifact
{
 using namespace Diligent;

 class IRenderer::Impl
 {
 private:

 public:
  explicit Impl();
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain_;

  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawRectOutline(float2 pos);
  void drawParticles();
 };

 IRenderer::IRenderer() :impl_(new Impl())
 {

 }

 IRenderer::IRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, RefCntAutoPtr<ISwapChain> pSwapChain) :impl_(new Impl())
 {

 }

 IRenderer::~IRenderer()
 {
  delete impl_;
 }


};

