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
import Core.Scale.Zoom;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;

 class AritfactIRenderer::Impl
 {
 private:
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
   Impl();
  ~Impl();
  void initialize(QWidget* parent);
  RefCntAutoPtr<IRenderDevice> pDevice_;
  RefCntAutoPtr<IDeviceContext> pImmediateContext_;
  RefCntAutoPtr<IDeviceContext> pDeferredContext_;
  RefCntAutoPtr<ISwapChain> pSwapChain_;
  ZoomScale2D zoom_;
  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;
  PSOAndSRB m_draw_line_pso_and_srb;
  PSOAndSRB m_draw_dot_line_pso_and_srb;
  PSOAndSRB m_draw_solid_rect_pso_and_srb;
  PSOAndSRB m_draw_sprite_pso_and_srb;
  int m_CurrentPhysicalWidth;
  int m_CurrentPhysicalHeight;
  int m_CurrentDevicePixelRatio;
  void clear();
  void flushAndWait();
  void createSwapChain(QWidget* widget);
  void recreateSwapChain(QWidget* widget);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSprite(const QImage& image);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawRectOutline(float2 pos,const FloatColor& color);
  void drawParticles();
 };

 AritfactIRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context, QWidget* widget) :pDevice_(device), pImmediateContext_(context)
 {
  createConstantBuffers();
  createShaders();
  createPSOs();

  

 }

 AritfactIRenderer::Impl::Impl()
 {

 }

 AritfactIRenderer::Impl::~Impl()
 {

 }

 void AritfactIRenderer::Impl::initialize(QWidget* parent)
 {
  //diligent engine directx12で初期化


 }

 void AritfactIRenderer::Impl::initContext(RefCntAutoPtr<IRenderDevice> device)
 {
  
  device->CreateDeferredContext(&pDeferredContext_);
 
 }

 void AritfactIRenderer::Impl::createShaders()
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

 void AritfactIRenderer::Impl::createPSOs()
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

void AritfactIRenderer::Impl::createConstantBuffers()
{
 BufferDesc VertDesc;
 VertDesc.Name = "Sprite vertex buffer";
 VertDesc.Usage = USAGE_IMMUTABLE; // 2Dスプライトなら基本書き換えない
 VertDesc.BindFlags = BIND_VERTEX_BUFFER;
 VertDesc.Size = sizeof(SpriteVertex);
 BufferData VBData(&m_draw_sprite_vertex_buffer, sizeof(SpriteVertex));
 pDevice_->CreateBuffer(VertDesc, &VBData, &m_draw_sprite_vertex_buffer);

 }

 
void AritfactIRenderer::Impl::createSwapChain(QWidget* window)
 {
 if (!window || !pDevice_)
 {

  return;
 }
   
 m_CurrentPhysicalWidth = static_cast<int>(window->width() * window->devicePixelRatio());
 m_CurrentPhysicalHeight = static_cast<int>(window->height() * window->devicePixelRatio());
 m_CurrentDevicePixelRatio = window->devicePixelRatio();
 // スワップチェインを作成
 SwapChainDesc SCDesc;
 SCDesc.Width = m_CurrentPhysicalWidth;  // QWindowの現在の幅
 SCDesc.Height = m_CurrentPhysicalHeight; // QWindowの現在の高さ
 SCDesc.ColorBufferFormat = MAIN_RTV_FORMAT;
 SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;

 SCDesc.BufferCount = 2;
 SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

 FullScreenModeDesc desc;

 desc.Fullscreen = false;
   
 }

 void AritfactIRenderer::Impl::recreateSwapChain(QWidget* widget)
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
 void AritfactIRenderer::Impl::drawSprite(const QImage& image)
 {
  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  RectVertex vertices[4] = {
  {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
  {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
  {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
  {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };
   
   
 }

 void AritfactIRenderer::Impl::clear()
 {

 }

 void AritfactIRenderer::Impl::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
 {
  RectVertex vertices[4] = {
	 {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	 {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	 {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	 {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

   
   
 }

 void AritfactIRenderer::Impl::flushAndWait()
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

 void AritfactIRenderer::Impl::captureScreenShot()
 {

 }

 AritfactIRenderer::AritfactIRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, QWidget* widget) :impl_(new Impl(pDevice, pImmediateContext,widget))
 {

 }

 AritfactIRenderer::AritfactIRenderer():impl_(new Impl())
 {

 }

 AritfactIRenderer::~AritfactIRenderer()
 {
  delete impl_;
 }

 void AritfactIRenderer::initialize(QWidget* widget)
 {
  impl_->initialize(widget);
 }

 void AritfactIRenderer::createSwapChain(QWidget* widget)
 {
  impl_->createSwapChain(widget);
 }
 void AritfactIRenderer::recreateSwapChain(QWidget* widget)
 {
  impl_->recreateSwapChain(widget);
 }

 void AritfactIRenderer::clear()
 {
  impl_->clear();
 }

 void AritfactIRenderer::flush()
 {
  
 }

 void AritfactIRenderer::flushAndWait()
 {
  impl_->flushAndWait();
 }



};

