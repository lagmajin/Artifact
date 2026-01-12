module;

#include <QImage>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>


module Artifact.Render.IRenderer;

import std;
import Graphics;
import Graphics.Shader.Set;
import Graphics.CBuffer.Constants;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;

 class IRenderer::Impl
 {
 private:
  PSOAndSRB m_draw_sprite_pso_and_srb;
  RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_sprite_index_buffer;
  RenderShaderPair m_draw_sprit_shaders;
  void initContext();
  void createShaders();
  void createPSOs();
 public:
   Impl();
   explicit Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context);
  ~Impl();
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<IDeviceContext> pDifferedContext;
  RefCntAutoPtr<ISwapChain> pSwapChain_;

  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSprite(const QImage& image);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawRectOutline(float2 pos);
  void drawParticles();
 };
 void IRenderer::Impl::initContext()
 {

 }

 void IRenderer::Impl::createShaders()
 {

 }

 void IRenderer::Impl::createPSOs()
 {

 }

 IRenderer::Impl::Impl()
 {

 }

 IRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context)
 {

 }

 IRenderer::Impl::~Impl()
 {

 }


 IRenderer::IRenderer() :impl_(new Impl())
 {

 }

 IRenderer::IRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, RefCntAutoPtr<ISwapChain> pSwapChain) :impl_(new Impl(pDevice, pImmediateContext))
 {

 }

 IRenderer::~IRenderer()
 {
  delete impl_;
 }


};

