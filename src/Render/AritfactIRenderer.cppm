module;
#include <QList>
#include <QImage>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
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
import VertexBuffer;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;

 class AritfactIRenderer::Impl
 {
 private:
  RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_sprite_index_buffer;
  RefCntAutoPtr<IBuffer> m_draw_sprite_cb;

  RefCntAutoPtr<IBuffer> m_draw_solid_rect_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_cb;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_trnsform_cb;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_index_buffer;
  RefCntAutoPtr<ITexture> m_layerRT;
  RenderShaderPair m_draw_sprite_shaders;
  RenderShaderPair m_draw_line_shaders;
  RenderShaderPair m_draw_outline_shaders;
  RenderShaderPair m_draw_solid_shaders;
  QWidget* widget_;
  QPointF pan_;
   
  bool m_initialized = false;
   
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
  RefCntAutoPtr<IRenderDevice>	pDevice_;
  RefCntAutoPtr<IDeviceContext> pImmediateContext_;
  RefCntAutoPtr<IDeviceContext> pDeferredContext_;
  RefCntAutoPtr<ISwapChain>		pSwapChain_;
  ZoomScale2D zoom_;
  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;
  PSOAndSRB m_draw_line_pso_and_srb;
  PSOAndSRB m_draw_dot_line_pso_and_srb;
  PSOAndSRB m_draw_solid_rect_pso_and_srb;
  PSOAndSRB m_draw_rect_outline_pso_and_srb;
  PSOAndSRB m_draw_sprite_pso_and_srb;
  RefCntAutoPtr<ISampler> m_draw_sprite_sampler;
  int m_CurrentPhysicalWidth;
  int m_CurrentPhysicalHeight;
  int m_CurrentDevicePixelRatio;
  void clear();
  void flushAndWait();
  void createSwapChain(QWidget* widget);
  void recreateSwapChain(QWidget* widget);
  void drawParticles();
  void drawRectOutline(float2 pos,const FloatColor& color);
  void drawSolidLine(float2 start, float2 end, const FloatColor& color, float thickness);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSprite(const QImage& image);
  void drawRectLocal(float x, float y, float w, float h, const FloatColor& color);
  void drawSpriteLocal(float x,float y,float w,float h,const QImage& image);
  void drawLineLocal(float2 p1, float2 p2, const FloatColor& color1,const FloatColor& color2);

  void destroy();
 };

 AritfactIRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context, QWidget* widget) :pDevice_(device), pImmediateContext_(context)
 {
  //createConstantBuffers();
  //createShaders();
  //createPSOs();

  

 }

 AritfactIRenderer::Impl::Impl()
 {

 }

 AritfactIRenderer::Impl::~Impl()
 {

 }

 void AritfactIRenderer::Impl::initialize(QWidget* widget)
 {
  //diligent engine directx12で初期化
  auto* pFactory = GetEngineFactoryD3D12();

  widget_ = widget;

  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;


  // ウィンドウハンドルを設定
  Win32NativeWindow hWindow;
  hWindow.hWnd = reinterpret_cast<HWND>(widget_->winId());
  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice_, &pImmediateContext_);
 
  if (!pDevice_)
  {
   // エラーログ出力、アプリケーション終了などの処理
   qWarning() << "Failed to create Diligent Engine device and contexts.";
   return;
  }
  m_CurrentPhysicalWidth = static_cast<int>(widget_->width() * widget_->devicePixelRatio());
  m_CurrentPhysicalHeight = static_cast<int>(widget_->height() * widget_->devicePixelRatio());
  m_CurrentDevicePixelRatio = widget_->devicePixelRatio();
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

  pFactory->CreateSwapChainD3D12(pDevice_, pImmediateContext_, SCDesc, desc, hWindow, &pSwapChain_);

  Diligent::Viewport VP;
  VP.Width = static_cast<float>(m_CurrentPhysicalWidth);
  VP.Height = static_cast<float>(m_CurrentPhysicalHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  // SetViewportsの最後の2引数は、レンダーターゲットの物理ピクセルサイズを渡すのが安全
  pImmediateContext_->SetViewports(1, &VP, m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);

  m_initialized = true;
  createConstantBuffers();
  createShaders();
  createPSOs();
 }

 void AritfactIRenderer::Impl::initContext(RefCntAutoPtr<IRenderDevice> device)
 {
  
  device->CreateDeferredContext(&pDeferredContext_);
 
 }

 void AritfactIRenderer::Impl::createShaders()
 {
  ShaderCreateInfo lineVsInfo;

  lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  lineVsInfo.EntryPoint = "main";
  lineVsInfo.Desc.Name = "LayerEditorVertexShader";
  lineVsInfo.Source = lineShaderVSText.constData();
  lineVsInfo.SourceLength = lineShaderVSText.length();

  ShaderCreateInfo linePsInfo;

  linePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  linePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  linePsInfo.Desc.Name = "LayerEditorVertexMyPixelShader";
  linePsInfo.Source = g_qsSolidColorPS2.constData();
  linePsInfo.SourceLength = g_qsSolidColorPS2.length();

  ShaderCreateInfo drawOutlineRectVsInfo;
  drawOutlineRectVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  drawOutlineRectVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  drawOutlineRectVsInfo.Desc.Name = "LayerEditorOutlineVertexShader";
  drawOutlineRectVsInfo.Source = drawOutlineRectVSSource.constData();
  drawOutlineRectVsInfo.SourceLength = drawOutlineRectVSSource.length();

  ShaderCreateInfo drawOutlineRectPsInfo;
  drawOutlineRectPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  drawOutlineRectPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  drawOutlineRectPsInfo.Desc.Name = "LayerEditorOutlinePixelShader";
  drawOutlineRectPsInfo.Source = drawOutlineRectPSSource.constData();
  drawOutlineRectPsInfo.SourceLength = drawOutlineRectPSSource.length();

  ShaderCreateInfo solidRectVsInfo;

  solidRectVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  solidRectVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  solidRectVsInfo.Desc.Name = "IRenderSolidRectVertexShader";
  solidRectVsInfo.Source = drawSolidRectVSSource.constData();
  solidRectVsInfo.SourceLength = drawSolidRectVSSource.length();

  ShaderCreateInfo solidPsInfo;

  solidPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  solidPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  solidPsInfo.Desc.Name = "IRenderSolidRectPixelShader";
  solidPsInfo.Source = g_qsSolidColorPSSource.constData();
  solidPsInfo.SourceLength = g_qsSolidColorPSSource.length();


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

  ShaderCreateInfo solidRectVsInfo2;
  solidRectVsInfo2.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  solidRectVsInfo2.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  solidRectVsInfo2.Desc.Name = "SolidRectVertexShader";
  solidRectVsInfo2.Source = drawSolidRectVSSource.constData();
  solidRectVsInfo2.SourceLength = drawSolidRectVSSource.length();

  ShaderCreateInfo solidRectPsInfo2;
  solidRectPsInfo2.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  solidRectPsInfo2.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  solidRectPsInfo2.Desc.Name = "SolidRectPixelShader";
  solidRectPsInfo2.Source = g_qsSolidColorPSSource.constData();
  solidRectPsInfo2.SourceLength = g_qsSolidColorPSSource.length();
   
     tbb::parallel_invoke(
         [&] { pDevice_->CreateShader(lineVsInfo, &m_draw_line_shaders.VS); },
         [&] { pDevice_->CreateShader(linePsInfo, &m_draw_line_shaders.PS); },
         [&] { pDevice_->CreateShader(sprite2DVsInfo, &m_draw_sprite_shaders.VS); },
         [&] { pDevice_->CreateShader(sprite2DPsInfo, &m_draw_sprite_shaders.PS); }
     );
     
     pDevice_->CreateShader(solidRectVsInfo2, &m_draw_solid_shaders.VS);
     pDevice_->CreateShader(solidRectPsInfo2, &m_draw_solid_shaders.PS);
   
 }

  void AritfactIRenderer::Impl::createPSOs()
  {
   GraphicsPipelineStateCreateInfo drawLinePSOCreateInfo;
   drawLinePSOCreateInfo.PSODesc.Name = "DrawLine PSO";
   drawLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
   LayoutElement lineLayoutElems[] = {
    LayoutElement{0, 0, 2, VT_FLOAT32, false},
    LayoutElement{1, 0, 4, VT_FLOAT32, false}
   };

   drawLinePSOCreateInfo.pVS = m_draw_line_shaders.VS;
   drawLinePSOCreateInfo.pPS = m_draw_line_shaders.PS;

   //pDevice_->CreateGraphicsPipelineState(drawLinePSOCreateInfo, &m_draw_line_pso_and_srb.pPSO);
   //m_draw_line_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_line_pso_and_srb.pSRB, true);

   GraphicsPipelineStateCreateInfo drawSolidRectPSOCreateInfo;
   drawSolidRectPSOCreateInfo.PSODesc.Name = "DrawSolidRect PSO";
   drawSolidRectPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
   LayoutElement solidRectLayoutElems[] = {
    LayoutElement{0, 0, 2, VT_FLOAT32, false},
    LayoutElement{1, 0, 4, VT_FLOAT32, false}
   };
   
   auto& GP = drawSolidRectPSOCreateInfo.GraphicsPipeline;
   GP.NumRenderTargets = 1;
   GP.RTVFormats[0] = MAIN_RTV_FORMAT; // 出力先RTVのフォーマットに合わせる
   GP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; // 矩形描画用
   GP.RasterizerDesc.CullMode = CULL_MODE_NONE;
   GP.DepthStencilDesc.DepthEnable = False;
   LayoutElement LayoutElems2[] =
   {
	   LayoutElement{0, 0, 2, VT_FLOAT32, false}, // pos: float2
	   LayoutElement{1, 0, 4, VT_FLOAT32, false}  // color: float4
   };
   GP.InputLayout.LayoutElements = LayoutElems2;
   GP.InputLayout.NumElements = _countof(LayoutElems2);
   ShaderResourceVariableDesc Vars2[] =
   {
	   { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
	   { SHADER_TYPE_PIXEL,  "ColorBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
   };
   drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars2;
   drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars2);


   drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = solidRectLayoutElems;
   drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(solidRectLayoutElems);

   drawSolidRectPSOCreateInfo.pVS = m_draw_solid_shaders.VS;
   drawSolidRectPSOCreateInfo.pPS = m_draw_solid_shaders.PS;

   pDevice_->CreateGraphicsPipelineState(drawSolidRectPSOCreateInfo, &m_draw_solid_rect_pso_and_srb.pPSO);
   m_draw_solid_rect_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_solid_rect_pso_and_srb.pSRB, true);


   GraphicsPipelineStateCreateInfo spritePSOCreateInfo;
      spritePSOCreateInfo.PSODesc.Name = "DrawSprite PSO";
	  spritePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
      LayoutElement spriteLayoutElems[] = {
       LayoutElement{0, 0, 2, VT_FLOAT32, false},
       LayoutElement{1, 0, 2, VT_FLOAT32, false},
       LayoutElement{2, 0, 4, VT_FLOAT32, false}
      };
      auto& GP2 = spritePSOCreateInfo.GraphicsPipeline;
      GP2.NumRenderTargets = 1;
      GP2.RTVFormats[0] = MAIN_RTV_FORMAT; // 出力先RTVのフォーマットに合わせる
      GP2.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; // 矩形描画用
      GP2.RasterizerDesc.CullMode = CULL_MODE_NONE;
      GP2.DepthStencilDesc.DepthEnable = False;
      GP2.InputLayout.LayoutElements = spriteLayoutElems;
      GP2.InputLayout.NumElements = _countof(spriteLayoutElems);
      spritePSOCreateInfo.pVS = m_draw_sprite_shaders.VS;
      spritePSOCreateInfo.pPS = m_draw_sprite_shaders.PS;
      pDevice_->CreateGraphicsPipelineState(spritePSOCreateInfo, &m_draw_sprite_pso_and_srb.pPSO);
	  m_draw_sprite_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_sprite_pso_and_srb.pSRB, true);

      SamplerDesc spriteSamplerDesc;
      spriteSamplerDesc.MinFilter = FILTER_TYPE_LINEAR;
      spriteSamplerDesc.MagFilter = FILTER_TYPE_LINEAR;
      spriteSamplerDesc.MipFilter = FILTER_TYPE_LINEAR;
      spriteSamplerDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
      spriteSamplerDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
      spriteSamplerDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
      spriteSamplerDesc.ComparisonFunc = COMPARISON_FUNC_ALWAYS;
      spriteSamplerDesc.MaxAnisotropy = 1;
      spriteSamplerDesc.MipLODBias = 0.0f;
      spriteSamplerDesc.MinLOD = 0.0f;
      spriteSamplerDesc.MaxLOD = FLT_MAX;
      pDevice_->CreateSampler(spriteSamplerDesc, &m_draw_sprite_sampler);


  }

void AritfactIRenderer::Impl::createConstantBuffers()
{
 {
  BufferDesc VertDesc;
  VertDesc.Name = "Sprite vertex buffer";
  VertDesc.Usage = USAGE_IMMUTABLE; // 2Dスプライトなら基本書き換えない
  VertDesc.BindFlags = BIND_VERTEX_BUFFER;
  VertDesc.Size = sizeof(SpriteVertex);
  BufferData VBData(&m_draw_sprite_vertex_buffer, sizeof(SpriteVertex));
  pDevice_->CreateBuffer(VertDesc, &VBData, &m_draw_sprite_vertex_buffer);

 }


 {
  BufferDesc CBDesc;
  CBDesc.Name = "VS Constants CB";
  CBDesc.Size = sizeof(Constants);
  CBDesc.Usage = USAGE_DYNAMIC;
  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

  pDevice_->CreateBuffer(CBDesc, nullptr, &m_draw_sprite_cb);
 
 }

 // DrawSolidRect用の定数バッファ (ColorBuffer)
 {
  BufferDesc CBDesc;
  CBDesc.Name = "DrawSolidColorCB";
  CBDesc.Usage = USAGE_DYNAMIC;
  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  CBDesc.Size = sizeof(DrawSpriteConstants);
  pDevice_->CreateBuffer(CBDesc, nullptr, &m_draw_solid_rect_cb);
 }

 // DrawSolidRect用の変換定数バッファ (TransformCB)
 {
  BufferDesc CBDesc;
  CBDesc.Name = "DrawSolidTransformCB";
  CBDesc.Usage = USAGE_DYNAMIC;
  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  CBDesc.Size = sizeof(CBSolidTransform2D);
  pDevice_->CreateBuffer(CBDesc, nullptr, &m_draw_solid_rect_trnsform_cb);
 }

 // DrawSolidRect用の頂点バッファ
 {
  BufferDesc vbDesc;
  vbDesc.Name = "SolidRect Vertex Buffer";
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Usage = USAGE_DYNAMIC;
  vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  vbDesc.Size = sizeof(RectVertex) * 4;
  pDevice_->CreateBuffer(vbDesc, nullptr, &m_draw_solid_rect_vertex_buffer);
 }

 // DrawSolidRect用のインデックスバッファ
 {
  uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };

  BufferDesc IndexBufferDesc;
  IndexBufferDesc.Name = "SolidRectIndexBuffer";
  IndexBufferDesc.Usage = USAGE_DEFAULT;
  IndexBufferDesc.BindFlags = BIND_INDEX_BUFFER;
  IndexBufferDesc.Size = sizeof(indices);
  IndexBufferDesc.CPUAccessFlags = CPU_ACCESS_NONE;

  BufferData InitData;
  InitData.pData = indices;
  InitData.DataSize = sizeof(indices);

  pDevice_->CreateBuffer(IndexBufferDesc, &InitData, &m_draw_solid_rect_index_buffer);
 }
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

 TextureDesc TexDesc;
 TexDesc.Name = "LayerRenderTarget";
 TexDesc.Type = RESOURCE_DIM_TEX_2D;
 TexDesc.Width = m_CurrentPhysicalWidth;
 TexDesc.Height = m_CurrentPhysicalHeight;
 TexDesc.MipLevels = 1;
 TexDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
 TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

 pDevice_->CreateTexture(TexDesc, nullptr, &m_layerRT);
   
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

  if (m_layerRT)
   m_layerRT.Release(); // 古いテクスチャを破棄

  TextureDesc TexDesc;
  TexDesc.Name = "LayerRenderTarget";
  TexDesc.Type = RESOURCE_DIM_TEX_2D;
  TexDesc.Width = newWidth;
  TexDesc.Height = newHeight;
  TexDesc.MipLevels = 1;
  TexDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

  pDevice_->CreateTexture(TexDesc, nullptr, &m_layerRT);
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
 void AritfactIRenderer::Impl::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
 {
  RectVertex vertices[4] = {
	 {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	 {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	 {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	 {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

  auto* pRTV = pSwapChain_->GetCurrentBackBufferRTV();

 }

 void AritfactIRenderer::Impl::drawSolidRect(float x, float y, float w, float h)
 {
  RectVertex vertices[4] = {
{{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
{{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
{{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
{{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };


 }
 void AritfactIRenderer::Impl::drawSprite(float2 pos, float2 size)
 {

 }

 void AritfactIRenderer::Impl::clear()
 {
  // クリアカラーの定義 (RGBA)
  float ClearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f }; // 赤

  // レンダリングターゲットのビューを取得
  auto* pRTV = pSwapChain_->GetCurrentBackBufferRTV();
  //auto* pDSV = pSwapChain_->GetDepthStencilView();
  pImmediateContext_->SetRenderTargets(1, &pRTV,nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  // クリアの実行
  pImmediateContext_->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


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

 void AritfactIRenderer::Impl::destroy()
 {
  // Diligent Engineリソースの解放（RefCntAutoPtrはnullptr代入で参照カウント減）
  m_draw_sprite_vertex_buffer = nullptr;
  m_draw_sprite_index_buffer = nullptr;
  m_draw_sprite_shaders.VS = nullptr;
  m_draw_sprite_shaders.PS = nullptr;
  m_draw_outline_shaders.VS = nullptr;
  m_draw_outline_shaders.PS = nullptr;
  m_draw_line_pso_and_srb.pPSO = nullptr;
  m_draw_line_pso_and_srb.pSRB = nullptr;
  m_draw_dot_line_pso_and_srb.pPSO = nullptr;
  m_draw_dot_line_pso_and_srb.pSRB = nullptr;
  m_draw_solid_rect_pso_and_srb.pPSO = nullptr;
  m_draw_solid_rect_pso_and_srb.pSRB = nullptr;
  m_draw_rect_outline_pso_and_srb.pPSO = nullptr;
  m_draw_rect_outline_pso_and_srb.pSRB = nullptr;
  m_draw_sprite_pso_and_srb.pPSO = nullptr;
  m_draw_sprite_pso_and_srb.pSRB = nullptr;
  pDevice_ = nullptr;
  pImmediateContext_ = nullptr;
  pDeferredContext_ = nullptr;
  pSwapChain_ = nullptr;
  widget_ = nullptr;
  m_initialized = false;
 }

 void AritfactIRenderer::Impl::drawSolidLine(float2 start, float2 end, const FloatColor& color, float thickness)
 {
  // TODO: Implementation needed
  // LineVertex v[4];
  // float2 d = normalize(end - start);
  // float2 n = float2(-d.y, d.x) * 0.5f;
 }

 void AritfactIRenderer::Impl::drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
 {
  if (!pSwapChain_) return;

  RectVertex vertices[4] = {
   {{0,0}, {color.r(), color.g(), color.b(), 1}}, // 左上
   {{w, 0.0f}, {color.r(), color.g(), color.b(), 1}}, // 右上
   {{0.0f, h}, {color.r(), color.g(), color.b(), 1}}, // 左下
   {{w, h}, {color.r(), color.g(), color.b(), 1}}, // 右下
  };

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  ITextureView* pLayerRTV = m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
  pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
   void* pData = nullptr;
   pImmediateContext_->MapBuffer(m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, vertices, sizeof(vertices));
   pImmediateContext_->UnmapBuffer(m_draw_solid_rect_vertex_buffer, MAP_WRITE);
  }

  {
   CBSolidColor cb = { {color.r(), color.g(), color.b(), 1.0f} };

   void* pData = nullptr;
   pImmediateContext_->MapBuffer(m_draw_solid_rect_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, &cb, sizeof(cb));
   pImmediateContext_->UnmapBuffer(m_draw_solid_rect_cb, MAP_WRITE);
  }

  {
   auto desc = pSwapChain_->GetDesc();
   float nx = (x + pan_.x()) / float(desc.Width) * 2.0f - 1.0f;
   float ny = 1.0f - (y + pan_.y()) / float(desc.Height) * 2.0f;

   CBSolidTransform2D cbTransform;
   cbTransform.offset = { x + (float)pan_.x(), y + (float)pan_.y() };
   cbTransform.scale = { 1,1 };
   cbTransform.screenSize = { float(desc.Width), float(desc.Height) };

   void* pData = nullptr;
   pImmediateContext_->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, &cbTransform, sizeof(cbTransform));
   pImmediateContext_->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
  }

  pImmediateContext_->SetPipelineState(m_draw_solid_rect_pso_and_srb.pPSO);

  IBuffer* pBuffers[] = { m_draw_solid_rect_vertex_buffer };
  Uint64 offsets[] = { 0 };
  pImmediateContext_->SetVertexBuffers(
   0,
   1,
   pBuffers,
   offsets,
   RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
   SET_VERTEX_BUFFERS_FLAG_RESET
  );

  pImmediateContext_->SetIndexBuffer(
   m_draw_solid_rect_index_buffer,
   0,
   RESOURCE_STATE_TRANSITION_MODE_TRANSITION
  );

  m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
  m_draw_solid_rect_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer")->Set(m_draw_solid_rect_cb);
  pImmediateContext_->CommitShaderResources(m_draw_solid_rect_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawIndexedAttribs drawAttrs(
   6,
   VT_UINT32,
   DRAW_FLAG_VERIFY_ALL
  );

  pImmediateContext_->DrawIndexed(drawAttrs);
 }

 void AritfactIRenderer::Impl::drawLineLocal(float2 p1, float2 p2, const FloatColor& color1, const FloatColor& color2)
 {
     if (!pSwapChain_) return;

     LineVertex vertices[2] = {
         {{p1.x, p1.y}, {color1.r(), color1.g(), color1.b(), 1}}, // 始点
         {{p2.x, p2.y}, {color2.r(), color2.g(), color2.b(), 1}}  // 終点
	 };

	 auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
	 ITextureView* pLayerRTV = m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
	 pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 }

 void AritfactIRenderer::Impl::drawSpriteLocal(float x, float y, float w, float h, const QImage& image)
 {
  if (!pSwapChain_) return;
  if (image.isNull()) return;
  if (w <= 0.0f || h <= 0.0f) return;

  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  if (rgba.isNull()) return;

  TextureDesc TexDesc;
  TexDesc.Name = "SpriteTexture";
  TexDesc.Type = RESOURCE_DIM_TEX_2D;
  TexDesc.Width = static_cast<Uint32>(rgba.width());
  TexDesc.Height = static_cast<Uint32>(rgba.height());
  TexDesc.MipLevels = 1;
  TexDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.Usage = USAGE_IMMUTABLE;
  TexDesc.BindFlags = BIND_SHADER_RESOURCE;

  TextureData InitData;
  TextureSubResData SubRes;
  SubRes.pData = rgba.constBits();
  SubRes.Stride = static_cast<Uint32>(rgba.bytesPerLine());
  InitData.pSubResources = &SubRes;
  InitData.NumSubresources = 1;

  RefCntAutoPtr<ITexture> spriteTexture;
  pDevice_->CreateTexture(TexDesc, &InitData, &spriteTexture);
  if (!spriteTexture)
  {
   qWarning() << "Failed to create sprite texture.";
   return;
  }

  ITextureView* spriteSRV = spriteTexture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
  if (!spriteSRV)
  {
   qWarning() << "Failed to get sprite texture SRV.";
   return;
  }

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const auto& desc = pSwapChain_->GetDesc();
  if (desc.Width == 0 || desc.Height == 0)
   return;

  float screenW = static_cast<float>(desc.Width);
  float screenH = static_cast<float>(desc.Height);
  float left = (x + pan_.x()) / screenW * 2.0f - 1.0f;
  float right = (x + w + pan_.x()) / screenW * 2.0f - 1.0f;
  float top = 1.0f - (y + pan_.y()) / screenH * 2.0f;
  float bottom = 1.0f - ((y + h) + pan_.y()) / screenH * 2.0f;

  struct SpriteVertexVL
  {
   float2 position;
   float2 uv;
   float4 color;
  };

  SpriteVertexVL vertices[4] = {
   {{left, top}, {0.0f, 0.0f}, {1, 1, 1, 1}},
   {{right, top}, {1.0f, 0.0f}, {1, 1, 1, 1}},
   {{left, bottom}, {0.0f, 1.0f}, {1, 1, 1, 1}},
   {{right, bottom}, {1.0f, 1.0f}, {1, 1, 1, 1}},
  };

  if (!m_draw_sprite_vertex_buffer)
  {
   qWarning() << "Sprite vertex buffer is not initialized.";
   return;
  }

  void* pData = nullptr;
  pImmediateContext_->MapBuffer(m_draw_sprite_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
  std::memcpy(pData, vertices, sizeof(vertices));
  pImmediateContext_->UnmapBuffer(m_draw_sprite_vertex_buffer, MAP_WRITE);

  IBuffer* buffers[] = { m_draw_sprite_vertex_buffer };
  Uint64 offsets[] = { 0 };
  pImmediateContext_->SetVertexBuffers(
   0,
   1,
   buffers,
   offsets,
   RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
   SET_VERTEX_BUFFERS_FLAG_RESET
  );

  pImmediateContext_->SetPipelineState(m_draw_sprite_pso_and_srb.pPSO);

  if (m_draw_sprite_pso_and_srb.pSRB)
  {
   auto textureVar = m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_texture");
   if (textureVar)
    textureVar->Set(spriteSRV);
   auto samplerVar = m_draw_sprite_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_sampler");
   if (samplerVar && m_draw_sprite_sampler)
    samplerVar->Set(m_draw_sprite_sampler);
   pImmediateContext_->CommitShaderResources(m_draw_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }

  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 4;
  drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
  pImmediateContext_->Draw(drawAttrs);
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
  impl_->pImmediateContext_->Flush();
 }

 void AritfactIRenderer::flushAndWait()
 {
  impl_->flushAndWait();
 }

 void AritfactIRenderer::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
 {

 }

 void AritfactIRenderer::drawSolidRect(float x, float y, float w, float h)
 {

 }

 void AritfactIRenderer::drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
 {
  impl_->drawRectLocal(x, y, w, h, color);
 }

 void AritfactIRenderer::present()
 {
  impl_->pSwapChain_->Present();
 }

 void AritfactIRenderer::destroy()
 {

 }

};

