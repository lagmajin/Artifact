module;
#include <QList>
#include <QImage>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <oneapi/tbb/tick_count.h>
#include <oneapi/tbb/parallel_invoke.h>


module Artifact.Render.IRenderer;

import std;
import Graphics;
import Graphics.Shader.Set;
//import Graphics.CBuffer.Constants;
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
  QWidget* widget_;
   
  void captureScreenShot();
   
  void initContext(RefCntAutoPtr<IRenderDevice> device);
  
  void createConstantBuffers();
  void createShaders();
  void createPSOs();
 public:
   explicit Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context,QWidget* widget);
  ~Impl();
  RefCntAutoPtr<IRenderDevice> pDevice_;
  RefCntAutoPtr<IDeviceContext> pImmediateContext_;
  RefCntAutoPtr<IDeviceContext> pDeferredContext_;
  RefCntAutoPtr<ISwapChain> pSwapChain_;
  void clear();
  void flushAndWait();
  void recreateSwapChain(QWidget* widget);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSprite(const QImage& image);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawRectOutline(float2 pos,const FloatColor& color);
  void drawParticles();
 };
 void IRenderer::Impl::initContext(RefCntAutoPtr<IRenderDevice> device)
 {
  
  device->CreateDeferredContext(&pDeferredContext_);
 
 }

 void IRenderer::Impl::createShaders()
 {
  ShaderCreateInfo sprite2DVsInfo;
  sprite2DVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  sprite2DVsInfo.Desc.Name = "SpriteVertexShader";

  sprite2DVsInfo.Source = g_qsBasic2DVS.constData();
  sprite2DVsInfo.SourceLength = g_qsBasic2DVS.length();


  ShaderCreateInfo sprite2DPsInfo;
  sprite2DPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  sprite2DPsInfo.Desc.Name = "SpritePixelShader";

  sprite2DPsInfo.Source = g_qsBasicSprite2DImagePS.constData();
  sprite2DPsInfo.SourceLength = g_qsBasicSprite2DImagePS.length();
   

   pDevice_->CreateShader(sprite2DVsInfo, &m_draw_sprit_shaders.VS);
   pDevice_->CreateShader(sprite2DPsInfo, &m_draw_sprit_shaders.PS);
   
 }

 void IRenderer::Impl::createPSOs()
 {
  GraphicsPipelineStateCreateInfo drawSpritePSOCreateInfo;
  drawSpritePSOCreateInfo.PSODesc.Name = "DrawSprite PSO";
  drawSpritePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
  LayoutElement LayoutElems[] = {
   // InputIndex, BufferSlot, NumComponents, ComponentType, IsNormalized
   LayoutElement{0, 0, 2, VT_FLOAT32, false}, // Attribute 0: pos (float2)
   LayoutElement{1, 0, 2, VT_FLOAT32, false}  // Attribute 1: uv (float2)
  };
   
  drawSpritePSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
  drawSpritePSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

  drawSpritePSOCreateInfo.pVS = m_draw_sprit_shaders.VS;
  drawSpritePSOCreateInfo.pPS = m_draw_sprit_shaders.PS;
   
   
  pDevice_->CreateGraphicsPipelineState(drawSpritePSOCreateInfo, &m_draw_sprite_pso_and_srb.pPSO);
   
 }

void IRenderer::Impl::createConstantBuffers()
{
 BufferDesc VertDesc;
 VertDesc.Name = "Sprite vertex buffer";
 VertDesc.Usage = USAGE_IMMUTABLE; // 2Dスプライトなら基本書き換えない
 VertDesc.BindFlags = BIND_VERTEX_BUFFER;
 VertDesc.Size = sizeof(SpriteVertex);
 BufferData VBData(&m_draw_sprite_vertex_buffer, sizeof(SpriteVertex));
 pDevice_->CreateBuffer(VertDesc, &VBData, &m_draw_sprite_vertex_buffer);

 }

 IRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context,QWidget*widget):pDevice_(device), pImmediateContext_(context)
 {
  createConstantBuffers();
  createShaders();
  createPSOs();
  
   
   
 }

 IRenderer::Impl::~Impl()
 {

 }
 void IRenderer::Impl::recreateSwapChain(QWidget* widget)
 {
  if (!widget || !pDevice_)
  {

   return;
  }


  //pSwapChain->Release();
  const int newWidth = static_cast<int>(widget->width() * widget->devicePixelRatio());
  const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
  const float newDevicePixelRatio = widget->devicePixelRatio();
  qDebug() << "Impl::recreateSwapChain - Logical:" << widget->width() << "x" << widget->height()
   << ", DPI:" << newDevicePixelRatio
   << ", Physical:" << newWidth << "x" << newHeight;
  qDebug() << "Before Resize - SwapChain Desc:" << pSwapChain_->GetDesc().Width << "x" << pSwapChain_->GetDesc().Height;
  pSwapChain_->Resize(newWidth, newHeight);


  qDebug() << "After Resize - SwapChain Desc:" << pSwapChain_->GetDesc().Width << "x" << pSwapChain_->GetDesc().Height;

  Diligent::Viewport VP;
  VP.Width = static_cast<float>(newWidth);  // newWidth, newHeightは物理ピクセル
  VP.Height = static_cast<float>(newHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  pImmediateContext_->SetViewports(1, &VP, newWidth, newHeight);

  qDebug() << "After SetViewports - Viewport WxH: " << VP.Width << "x" << VP.Height;
  qDebug() << "After SetViewports - Viewport TopLeftXY: " << VP.TopLeftX << ", " << VP.TopLeftY;

  //calcProjection(m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);
 }
 void IRenderer::Impl::drawSprite(const QImage& image)
 {
  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  RectVertex vertices[4] = {
  {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
  {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
  {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
  {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };
   
   
 }

 void IRenderer::Impl::clear()
 {

 }

 void IRenderer::Impl::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
 {
  RectVertex vertices[4] = {
	 {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	 {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	 {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	 {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

   
   
 }

 void IRenderer::Impl::flushAndWait()
 {
  RefCntAutoPtr<IFence> fence;
  FenceDesc desc;
  desc.Name = "StopRenderLoopFence";
  desc.Type = FENCE_TYPE_GENERAL;

  pDevice_->CreateFence(desc, &fence);

  pImmediateContext_->Flush();
  pImmediateContext_->EnqueueSignal(fence, 1);
  pImmediateContext_->Flush();

  fence->Wait(1);
 }

 void IRenderer::Impl::captureScreenShot()
 {

 }



 IRenderer::IRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, QWidget* widget) :impl_(new Impl(pDevice, pImmediateContext,widget))
 {

 }

 IRenderer::~IRenderer()
 {
  delete impl_;
 }

 void IRenderer::recreateSwapChain(QWidget* widget)
 {
  impl_->recreateSwapChain(widget);
 }

 void IRenderer::clear()
 {

 }

 void IRenderer::flush()
 {

 }

 void IRenderer::flushAndWait()
 {
  impl_->flushAndWait();
 }

};

