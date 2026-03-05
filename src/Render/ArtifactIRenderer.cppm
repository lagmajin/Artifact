module;
#include <QList>
#include <QImage>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Query.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
//#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/TextureD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <oneapi/tbb/tick_count.h>
#include <oneapi/tbb/parallel_invoke.h>
#include <windows.h>


module Artifact.Render.IRenderer;

import std;
import Graphics;
import Graphics.Shader.Set;
//import Graphics.CBuffer.Constants;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Core.Scale.Zoom;
import Core.Transform.Viewport;
import Math.Bezier;
import VertexBuffer;
import Render.Shader.ThickLine;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;

 class ArtifactIRenderer::Impl
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
  RenderShaderPair m_draw_thick_line_shaders;
  RenderShaderPair m_draw_dot_line_shaders;
  RenderShaderPair m_draw_solid_triangle_shaders;
 
  RefCntAutoPtr<IBuffer> m_draw_thick_line_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_dot_line_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_solid_triangle_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_dot_line_cb;
  QWidget* widget_;
  HWND renderHwnd_ = nullptr;
  ViewportTransformer viewport_;

  bool m_initialized = false;
 bool m_frameQueryInitialized = false;
 double m_lastGpuFrameTimeMs = 0.0;
 Uint32 m_frameQueryIndex = 0;
 static constexpr Uint32 FrameQueryCount = 2;
 std::array<RefCntAutoPtr<IQuery>, FrameQueryCount> m_frameQueries;
   
  void captureScreenShot();
   
  void initContext(RefCntAutoPtr<IRenderDevice> device);
  
  void createConstantBuffers();
  void createShaders();
  void createPSOs();
  void initFrameQueries();
 public:
   explicit Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context,QWidget* widget);
   Impl();
  ~Impl();
  void initialize(QWidget* parent);
  RefCntAutoPtr<IRenderDevice>	pDevice_;
  RefCntAutoPtr<IDeviceContext> pImmediateContext_;
  RefCntAutoPtr<IDeviceContext> pDeferredContext_;
  RefCntAutoPtr<ISwapChain>		pSwapChain_;
  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;
  PSOAndSRB m_draw_line_pso_and_srb;
  PSOAndSRB m_draw_dot_line_pso_and_srb;
  PSOAndSRB m_draw_solid_rect_pso_and_srb;
  PSOAndSRB m_draw_rect_outline_pso_and_srb;
  PSOAndSRB m_draw_sprite_pso_and_srb;
  PSOAndSRB m_draw_thick_line_pso_and_srb;
  PSOAndSRB m_draw_solid_triangle_pso_and_srb;
  RefCntAutoPtr<ISampler> m_draw_sprite_sampler;
  int m_CurrentPhysicalWidth;
  int m_CurrentPhysicalHeight;
  int m_CurrentDevicePixelRatio;
  void clear();
  void flushAndWait();
  void createSwapChain(QWidget* widget);
  void recreateSwapChain(QWidget* widget);
  void beginFrameGpuProfiling();
  void endFrameGpuProfiling();
  double lastFrameGpuTimeMs() const;
  void drawParticles();
   void drawRectOutline(float x, float y, float w, float h, const FloatColor& color);
   void drawRectOutline(float2 pos, float2 size, const FloatColor& color);
  void drawSolidLine(float2 start, float2 end, const FloatColor& color, float thickness);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float2 pos, float2 size);
  void drawSprite(const QImage& image);
  void drawRectLocal(float x, float y, float w, float h, const FloatColor& color);
  void drawSpriteLocal(float x,float y,float w,float h,const QImage& image);
  void drawLineLocal(float2 p1, float2 p2, const FloatColor& color1,const FloatColor& color2);
  void drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color);
  void drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color);
  void drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color);
  void drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color);
  void drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color);
  void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color);

  void setViewportSize(float w, float h) { viewport_.SetViewportSize(w, h); }
  void setCanvasSize(float w, float h) { viewport_.SetCanvasSize(w, h); }
  void setPan(float x, float y) { viewport_.SetPan(x, y); }
  void setZoom(float zoom) { viewport_.SetZoom(zoom); }
  void panBy(float dx, float dy) { viewport_.PanBy(dx, dy); }
  void resetView() { viewport_.ResetView(); }
  void fitToViewport(float margin) { viewport_.FitCanvasToViewport(margin); }
  void zoomAroundViewportPoint(float2 viewportPos, float newZoom) { viewport_.ZoomAroundViewportPoint(viewportPos, newZoom); }
  float2 canvasToViewport(float2 pos) const { return viewport_.CanvasToViewport(pos); }
  float2 viewportToCanvas(float2 pos) const { return viewport_.ViewportToCanvas(pos); }
 
  void destroy();
 };

 ArtifactIRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device, RefCntAutoPtr<IDeviceContext>& context, QWidget* widget) :pDevice_(device), pImmediateContext_(context)
 {
  //createConstantBuffers();
  //createShaders();
  //createPSOs();

  

 }

 ArtifactIRenderer::Impl::Impl()
 {

 }

 ArtifactIRenderer::Impl::~Impl()
 {

 }

 void ArtifactIRenderer::Impl::initialize(QWidget* widget)
 {
  //diligent engine directx12で初期化
  auto* pFactory = GetEngineFactoryD3D12();

  widget_ = widget;

  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;


  // ウィンドウハンドルを設定（デバイス作成のみ使用。スワップチェーンは子HWNDを使う）
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

  // DXGI の Present() がバックグラウンドスレッドからウィンドウメッセージをポンプしても
  // Qtの内部状態に触れないよう、スワップチェーン専用の子HWNDを作成する
  HWND parentHwnd = reinterpret_cast<HWND>(widget_->winId());
  renderHwnd_ = CreateWindowEx(
	  0, L"STATIC", nullptr,
	  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
	  0, 0, m_CurrentPhysicalWidth, m_CurrentPhysicalHeight,
	  parentHwnd, nullptr, GetModuleHandle(nullptr), nullptr);

  // スワップチェインを作成
  SwapChainDesc SCDesc;
  SCDesc.Width = m_CurrentPhysicalWidth;  // QWindowの現在の幅
  SCDesc.Height = m_CurrentPhysicalHeight; // QWindowの現在の高さ
  SCDesc.ColorBufferFormat = MAIN_RTV_FORMAT;
  SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;

  SCDesc.BufferCount = 2;
  SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

  FullScreenModeDesc fullScreenDesc;
  fullScreenDesc.Fullscreen = false;

  Win32NativeWindow swapChainWindow;
  swapChainWindow.hWnd = renderHwnd_;
  pFactory->CreateSwapChainD3D12(pDevice_, pImmediateContext_, SCDesc, fullScreenDesc, swapChainWindow, &pSwapChain_);

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

 void ArtifactIRenderer::Impl::initFrameQueries()
 {
  if (m_frameQueryInitialized || !pDevice_)
   return;

  QueryDesc desc;
  desc.Name = "FrameDurationQuery";
  desc.Type = QUERY_TYPE_DURATION;

  for (auto& query : m_frameQueries)
  {
   pDevice_->CreateQuery(desc, &query);
  }

  m_frameQueryInitialized = true;
 }

 void ArtifactIRenderer::Impl::initContext(RefCntAutoPtr<IRenderDevice> device)
 {
  
  device->CreateDeferredContext(&pDeferredContext_);
 
 }

 void ArtifactIRenderer::Impl::createShaders()
 {
  ShaderCreateInfo lineVsInfo;

  lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  lineVsInfo.EntryPoint = "main";
  lineVsInfo.Desc.Name = "LayerEditorVertexShader";
  lineVsInfo.Source = g_thickLineVS.constData();
  lineVsInfo.SourceLength = g_thickLineVS.length();

  ShaderCreateInfo linePsInfo;

  linePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  linePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  linePsInfo.Desc.Name = "LayerEditorVertexMyPixelShader";
  linePsInfo.Source = g_thickLinePS.constData();
  linePsInfo.SourceLength = g_thickLinePS.length();

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

  sprite2DVsInfo.Source = g_2DSpriteVS.constData();
  sprite2DVsInfo.SourceLength = g_2DSpriteVS.length();


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
	 m_draw_solid_triangle_shaders = m_draw_solid_shaders;

  {
   ShaderCreateInfo thickLineVsInfo;
   thickLineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
   thickLineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
   thickLineVsInfo.Desc.Name = "ThickLineVertexShader";
   thickLineVsInfo.Source = g_thickLineVS.constData();
   thickLineVsInfo.SourceLength = g_thickLineVS.length();

   ShaderCreateInfo thickLinePsInfo;
   thickLinePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
   thickLinePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
   thickLinePsInfo.Desc.Name = "ThickLinePixelShader";
   thickLinePsInfo.Source = g_thickLinePS.constData();
   thickLinePsInfo.SourceLength = g_thickLinePS.length();

   pDevice_->CreateShader(thickLineVsInfo, &m_draw_thick_line_shaders.VS);
   pDevice_->CreateShader(thickLinePsInfo, &m_draw_thick_line_shaders.PS);
  }
 
  {
   ShaderCreateInfo dotLineVsInfo;
   dotLineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
   dotLineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
   dotLineVsInfo.Desc.Name = "DotLineVertexShader";
   dotLineVsInfo.Source = g_dotLineVS.constData();
   dotLineVsInfo.SourceLength = g_dotLineVS.length();
 
   ShaderCreateInfo dotLinePsInfo;
   dotLinePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
   dotLinePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
   dotLinePsInfo.Desc.Name = "DotLinePixelShader";
   dotLinePsInfo.Source = g_dotLinePS.constData();
   dotLinePsInfo.SourceLength = g_dotLinePS.length();
 
   pDevice_->CreateShader(dotLineVsInfo, &m_draw_dot_line_shaders.VS);
   pDevice_->CreateShader(dotLinePsInfo, &m_draw_dot_line_shaders.PS);
  }
 }

  void ArtifactIRenderer::Impl::createPSOs()
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

   auto& LGP = drawLinePSOCreateInfo.GraphicsPipeline;
   LGP.NumRenderTargets = 1;
   LGP.RTVFormats[0] = MAIN_RTV_FORMAT;
   LGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_LIST;
   LGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
   LGP.DepthStencilDesc.DepthEnable = False;
   LGP.InputLayout.LayoutElements = lineLayoutElems;
   LGP.InputLayout.NumElements = 2;

   ShaderResourceVariableDesc Vars_Line[] = {
	   { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
   };
   drawLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars_Line;
   drawLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars_Line);

   pDevice_->CreateGraphicsPipelineState(drawLinePSOCreateInfo, &m_draw_line_pso_and_srb.pPSO);
   if (m_draw_line_pso_and_srb.pPSO)
   {
	m_draw_line_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_line_pso_and_srb.pSRB, true);
   }

   GraphicsPipelineStateCreateInfo drawSolidRectPSOCreateInfo;
   drawSolidRectPSOCreateInfo.PSODesc.Name = "DrawSolidRect PSO";
   drawSolidRectPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
   LayoutElement solidRectLayoutElems[] = {
	LayoutElement{0, 0, 2, VT_FLOAT32, false},
	LayoutElement{1, 0, 4, VT_FLOAT32, false}
   };

   auto& GP = drawSolidRectPSOCreateInfo.GraphicsPipeline;
   GP.NumRenderTargets = 1;
   GP.RTVFormats[0] = MAIN_RTV_FORMAT;
   GP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   GP.RasterizerDesc.CullMode = CULL_MODE_NONE;
   GP.DepthStencilDesc.DepthEnable = False;
   LayoutElement LayoutElems2[] =
   {
	   LayoutElement{0, 0, 2, VT_FLOAT32, false},
	   LayoutElement{1, 0, 4, VT_FLOAT32, false}
   };
   GP.InputLayout.LayoutElements = LayoutElems2;
   GP.InputLayout.NumElements = 2;
   ShaderResourceVariableDesc Vars2[] =
   {
	   { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
	   { SHADER_TYPE_PIXEL,  "ColorBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
   };
   drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars2;
   drawSolidRectPSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars2);

   drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = solidRectLayoutElems;
   drawSolidRectPSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = 2;

   drawSolidRectPSOCreateInfo.pVS = m_draw_solid_shaders.VS;
   drawSolidRectPSOCreateInfo.pPS = m_draw_solid_shaders.PS;

   pDevice_->CreateGraphicsPipelineState(drawSolidRectPSOCreateInfo, &m_draw_solid_rect_pso_and_srb.pPSO);
   if (m_draw_solid_rect_pso_and_srb.pPSO)
   {
	m_draw_solid_rect_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_solid_rect_pso_and_srb.pSRB, true);
   }
 
   GraphicsPipelineStateCreateInfo drawSolidTrianglePSOCreateInfo = drawSolidRectPSOCreateInfo;
   drawSolidTrianglePSOCreateInfo.PSODesc.Name = "DrawSolidTriangle PSO";
   drawSolidTrianglePSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   pDevice_->CreateGraphicsPipelineState(drawSolidTrianglePSOCreateInfo, &m_draw_solid_triangle_pso_and_srb.pPSO);
   if (m_draw_solid_triangle_pso_and_srb.pPSO)
   {
    m_draw_solid_triangle_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_solid_triangle_pso_and_srb.pSRB, true);
   }


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
	  GP2.RTVFormats[0] = MAIN_RTV_FORMAT;
	  GP2.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	  GP2.RasterizerDesc.CullMode = CULL_MODE_NONE;
	  GP2.DepthStencilDesc.DepthEnable = False;
	  GP2.InputLayout.LayoutElements = spriteLayoutElems;
	  GP2.InputLayout.NumElements = 3;
	  ShaderResourceVariableDesc spriteVars[] = {
	   { SHADER_TYPE_PIXEL, "g_texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
	   { SHADER_TYPE_PIXEL, "g_sampler", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
	  };
	  spritePSOCreateInfo.PSODesc.ResourceLayout.Variables = spriteVars;
	  spritePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(spriteVars);
	  spritePSOCreateInfo.pVS = m_draw_sprite_shaders.VS;
	  spritePSOCreateInfo.pPS = m_draw_sprite_shaders.PS;
	  pDevice_->CreateGraphicsPipelineState(spritePSOCreateInfo, &m_draw_sprite_pso_and_srb.pPSO);
	  if (m_draw_sprite_pso_and_srb.pPSO)
	  {
	   m_draw_sprite_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_sprite_pso_and_srb.pSRB, true);
	  }

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

  // ThickLine PSO
  {
   GraphicsPipelineStateCreateInfo thickLinePSOCreateInfo;
   thickLinePSOCreateInfo.PSODesc.Name = "DrawThickLine PSO";
   thickLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

   LayoutElement thickLineLayoutElems[] = {
	LayoutElement{0, 0, 2, VT_FLOAT32, false},
	LayoutElement{1, 0, 4, VT_FLOAT32, false}
   };

   auto& TLGP = thickLinePSOCreateInfo.GraphicsPipeline;
   TLGP.NumRenderTargets = 1;
   TLGP.RTVFormats[0] = MAIN_RTV_FORMAT;
   TLGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   TLGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
   TLGP.DepthStencilDesc.DepthEnable = False;
   TLGP.InputLayout.LayoutElements = thickLineLayoutElems;
   TLGP.InputLayout.NumElements = 2;

   ShaderResourceVariableDesc thickLineVars[] = {
	{ SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
   };
   thickLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = thickLineVars;
   thickLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 1;

   thickLinePSOCreateInfo.pVS = m_draw_thick_line_shaders.VS;
   thickLinePSOCreateInfo.pPS = m_draw_thick_line_shaders.PS;

   pDevice_->CreateGraphicsPipelineState(thickLinePSOCreateInfo, &m_draw_thick_line_pso_and_srb.pPSO);
   if (m_draw_thick_line_pso_and_srb.pPSO)
   {
	m_draw_thick_line_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_thick_line_pso_and_srb.pSRB, true);
   }
  }
 
  // DotLine PSO (Task 3)
  {
   GraphicsPipelineStateCreateInfo dotLinePSOCreateInfo;
   dotLinePSOCreateInfo.PSODesc.Name = "DrawDotLine PSO";
   dotLinePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
 
   LayoutElement dotLineLayoutElems[] = {
 	 LayoutElement{0, 0, 2, VT_FLOAT32, false},
 	 LayoutElement{1, 0, 4, VT_FLOAT32, false},
     LayoutElement{2, 0, 1, VT_FLOAT32, false}
   };
 
   auto& DLGP = dotLinePSOCreateInfo.GraphicsPipeline;
   DLGP.NumRenderTargets = 1;
   DLGP.RTVFormats[0] = MAIN_RTV_FORMAT;
   DLGP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   DLGP.RasterizerDesc.CullMode = CULL_MODE_NONE;
   DLGP.DepthStencilDesc.DepthEnable = False;
   DLGP.InputLayout.LayoutElements = dotLineLayoutElems;
   DLGP.InputLayout.NumElements = 3;
 
   ShaderResourceVariableDesc dotLineVars[] = {
 	 { SHADER_TYPE_VERTEX, "TransformCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC },
     { SHADER_TYPE_PIXEL,  "DotLineCB",   SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
   };
   dotLinePSOCreateInfo.PSODesc.ResourceLayout.Variables = dotLineVars;
   dotLinePSOCreateInfo.PSODesc.ResourceLayout.NumVariables = 2;
 
   dotLinePSOCreateInfo.pVS = m_draw_dot_line_shaders.VS;
   dotLinePSOCreateInfo.pPS = m_draw_dot_line_shaders.PS;
 
   pDevice_->CreateGraphicsPipelineState(dotLinePSOCreateInfo, &m_draw_dot_line_pso_and_srb.pPSO);
   if (m_draw_dot_line_pso_and_srb.pPSO)
   {
 	 m_draw_dot_line_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_dot_line_pso_and_srb.pSRB, true);
   }
  }

  }

 void ArtifactIRenderer::Impl::beginFrameGpuProfiling()
 {
  initFrameQueries();
  auto& query = m_frameQueries[m_frameQueryIndex];
  if (!query || !pImmediateContext_)
   return;

  pImmediateContext_->BeginQuery(query);
 }

 void ArtifactIRenderer::Impl::endFrameGpuProfiling()
 {
  auto& query = m_frameQueries[m_frameQueryIndex];
  if (!query || !pImmediateContext_)
   return;

  pImmediateContext_->EndQuery(query);

  const Uint32 readIndex = (m_frameQueryIndex + FrameQueryCount - 1) % FrameQueryCount;
  auto& readQuery = m_frameQueries[readIndex];
  if (readQuery)
  {
   QueryDataDuration data;
   if (readQuery->GetData(&data, sizeof(data), True) && data.Frequency != 0)
   {
    m_lastGpuFrameTimeMs = static_cast<double>(data.Duration) * 1000.0 / static_cast<double>(data.Frequency);
   }
  }

  m_frameQueryIndex = (m_frameQueryIndex + 1) % FrameQueryCount;
 }

 double ArtifactIRenderer::Impl::lastFrameGpuTimeMs() const
 {
  return m_lastGpuFrameTimeMs;
 }

void ArtifactIRenderer::Impl::createConstantBuffers()
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
 
  vbDesc.Name = "SolidTriangle Vertex Buffer";
  vbDesc.Size = sizeof(RectVertex) * 3;
  pDevice_->CreateBuffer(vbDesc, nullptr, &m_draw_solid_triangle_vertex_buffer);
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

 // ThickLine用の頂点バッファ (4頂点 TRIANGLE_STRIP)
 {
  BufferDesc vbDesc;
  vbDesc.Name = "ThickLine Vertex Buffer";
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Usage = USAGE_DYNAMIC;
  vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  vbDesc.Size = sizeof(RectVertex) * 4;
  pDevice_->CreateBuffer(vbDesc, nullptr, &m_draw_thick_line_vertex_buffer);
 }
 
 // DotLine用の頂点バッファ
 {
  BufferDesc vbDesc;
  vbDesc.Name = "DotLine Vertex Buffer";
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  vbDesc.Usage = USAGE_DYNAMIC;
  vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  vbDesc.Size = sizeof(DotLineVertex) * 4;
  pDevice_->CreateBuffer(vbDesc, nullptr, &m_draw_dot_line_vertex_buffer);
 }
 
 // DotLine用定数バッファ
 {
  struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
  BufferDesc CBDesc;
  CBDesc.Name = "DotLineParamsCB";
  CBDesc.Usage = USAGE_DYNAMIC;
  CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
  CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  CBDesc.Size = sizeof(DotLineShaderCB);
  pDevice_->CreateBuffer(CBDesc, nullptr, &m_draw_dot_line_cb);
 }
 }

 
void ArtifactIRenderer::Impl::createSwapChain(QWidget* window)
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

 void ArtifactIRenderer::Impl::recreateSwapChain(QWidget* widget)
 {
  if (!widget || !pDevice_ || !pSwapChain_)
  {

   return;
  }


  //pSwapChain->Release();
  const int newWidth = static_cast<int>(widget->width() * widget->devicePixelRatio());
  const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
  const float newDevicePixelRatio = widget->devicePixelRatio();
  m_CurrentPhysicalWidth = newWidth;
  m_CurrentPhysicalHeight = newHeight;
  m_CurrentDevicePixelRatio = static_cast<int>(newDevicePixelRatio);
  qDebug() << "Impl::recreateSwapChain - Logical:" << widget->width() << "x" << widget->height()
   << ", DPI:" << newDevicePixelRatio
   << ", Physical:" << newWidth << "x" << newHeight;
  qDebug() << "Before Resize - SwapChain Desc:" << pSwapChain_->GetDesc().Width << "x" << pSwapChain_->GetDesc().Height;
  pSwapChain_->Resize(newWidth, newHeight);
  if (renderHwnd_)
   SetWindowPos(renderHwnd_, nullptr, 0, 0, newWidth, newHeight,
				SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE);

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
 void ArtifactIRenderer::Impl::drawSprite(const QImage& image)
 {
  QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
  RectVertex vertices[4] = {
  {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
  {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
  {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
  {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };


   
   
 }
  void ArtifactIRenderer::Impl::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
  {
   drawRectLocal(pos.x, pos.y, size.x, size.y, color);
  }

  void ArtifactIRenderer::Impl::drawSolidRect(float x, float y, float w, float h)
  {
   drawRectLocal(x, y, w, h, {1.0f, 1.0f, 1.0f, 1.0f}); // Default to white
  }
 void ArtifactIRenderer::Impl::drawSprite(float2 pos, float2 size)
 {

 }

 void ArtifactIRenderer::Impl::clear()
 {
 if (!pSwapChain_ || !pImmediateContext_) return;
  // クリアカラーの定義 (RGBA)
  float ClearColor[] = { 0.10f, 0.10f, 0.10f, 1.0f };

  // レンダリングターゲットのビューを取得
  auto* pRTV = pSwapChain_->GetCurrentBackBufferRTV();
  //auto* pDSV = pSwapChain_->GetDepthStencilView();
  pImmediateContext_->SetRenderTargets(1, &pRTV,nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  // クリアの実行
  pImmediateContext_->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


 }

 
 void ArtifactIRenderer::Impl::flushAndWait()
 {
  if (!pDevice_ || !pImmediateContext_) return;
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

 void ArtifactIRenderer::Impl::captureScreenShot()
 {

 }

 void ArtifactIRenderer::Impl::destroy()
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
  m_draw_thick_line_pso_and_srb.pPSO = nullptr;
  m_draw_thick_line_pso_and_srb.pSRB = nullptr;
  m_draw_thick_line_vertex_buffer = nullptr;
  m_draw_thick_line_shaders.VS = nullptr;
  m_draw_thick_line_shaders.PS = nullptr;
  m_draw_dot_line_vertex_buffer = nullptr;
  m_draw_dot_line_cb = nullptr;
  m_draw_dot_line_shaders.VS = nullptr;
  m_draw_dot_line_shaders.PS = nullptr;
  pDevice_ = nullptr;
  pImmediateContext_ = nullptr;
  pDeferredContext_ = nullptr;
  pSwapChain_ = nullptr;
  if (renderHwnd_) {
   DestroyWindow(renderHwnd_);
   renderHwnd_ = nullptr;
  }
  widget_ = nullptr;
  m_initialized = false;
 }

  void ArtifactIRenderer::Impl::drawSolidLine(float2 start, float2 end, const FloatColor& color, float thickness)
  {
   drawThickLineLocal(start, end, thickness, color);
  }

 void ArtifactIRenderer::Impl::drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
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
   float nx = (x + viewport_.GetPan().x) / float(desc.Width) * 2.0f - 1.0f;
   float ny = 1.0f - (y + viewport_.GetPan().y) / float(desc.Height) * 2.0f;

   CBSolidTransform2D cbTransform;
   cbTransform.offset = { x + viewport_.GetPan().x, y + viewport_.GetPan().y };
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

 void ArtifactIRenderer::Impl::drawLineLocal(float2 p1, float2 p2, const FloatColor& color1, const FloatColor& color2)
 {
     if (!pSwapChain_) return;

     LineVertex vertices[2] = {
         {{p1.x, p1.y}, {color1.r(), color1.g(), color1.b(), 1}}, // 始点
         {{p2.x, p2.y}, {color2.r(), color2.g(), color2.b(), 1}}  // 終点
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
	  auto viewportCB = viewport_.GetViewportCB();
	  CBSolidTransform2D cbTransform;
	  cbTransform.offset = viewportCB.offset;
	  cbTransform.scale = viewportCB.scale;
	  cbTransform.screenSize = viewportCB.screenSize;
 
	  void* pData = nullptr;
	  pImmediateContext_->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
	  std::memcpy(pData, &cbTransform, sizeof(cbTransform));
	  pImmediateContext_->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
	 }

	 pImmediateContext_->SetPipelineState(m_draw_line_pso_and_srb.pPSO);

	 IBuffer* pBuffers[] = { m_draw_solid_rect_vertex_buffer };
	 Uint64 offsets[] = { 0 };
	 pImmediateContext_->SetVertexBuffers(0, 1, pBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

	 m_draw_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
	 pImmediateContext_->CommitShaderResources(m_draw_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

	 DrawAttribs drawAttrs;
	 drawAttrs.NumVertices = 2;
	 drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
	 pImmediateContext_->Draw(drawAttrs);
 }

 void ArtifactIRenderer::Impl::drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color)
 {
  if (!pSwapChain_ || !m_draw_thick_line_pso_and_srb.pPSO) return;
  if (thickness <= 0.0f) return;

  float2 d = { p2.x - p1.x, p2.y - p1.y };
  float len = std::sqrt(d.x * d.x + d.y * d.y);
  if (len < 1e-5f) return;

  float2 nd = { d.x / len, d.y / len };
  float half = thickness * 0.5f;
  float2 n = { -nd.y * half, nd.x * half };

  float4 c = { color.r(), color.g(), color.b(), 1.0f };
  RectVertex vertices[4] = {
   { { p1.x + n.x, p1.y + n.y }, c },
   { { p1.x - n.x, p1.y - n.y }, c },
   { { p2.x + n.x, p2.y + n.y }, c },
   { { p2.x - n.x, p2.y - n.y }, c },
  };

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
   void* pData = nullptr;
   pImmediateContext_->MapBuffer(m_draw_thick_line_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, vertices, sizeof(vertices));
   pImmediateContext_->UnmapBuffer(m_draw_thick_line_vertex_buffer, MAP_WRITE);
  }

  {
   auto viewportCB = viewport_.GetViewportCB();
   CBSolidTransform2D cbTransform;
   cbTransform.offset = viewportCB.offset;
   cbTransform.scale = viewportCB.scale;
   cbTransform.screenSize = viewportCB.screenSize;
 
   void* pData = nullptr;
   pImmediateContext_->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, &cbTransform, sizeof(cbTransform));
   pImmediateContext_->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
  }

  pImmediateContext_->SetPipelineState(m_draw_thick_line_pso_and_srb.pPSO);

  IBuffer* pBuffers[] = { m_draw_thick_line_vertex_buffer };
  Uint64 offsets[] = { 0 };
  pImmediateContext_->SetVertexBuffers(0, 1, pBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

  m_draw_thick_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
  pImmediateContext_->CommitShaderResources(m_draw_thick_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 4;
  drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
  pImmediateContext_->Draw(drawAttrs);
 }

  void ArtifactIRenderer::Impl::drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color)
  {
   if (!pSwapChain_ || !m_draw_dot_line_pso_and_srb.pPSO) return;
   if (thickness <= 0.0f) return;
 
   float2 d = { p2.x - p1.x, p2.y - p1.y };
   float len = std::sqrt(d.x * d.x + d.y * d.y);
   if (len < 1e-5f) return;
 
   float2 nd = { d.x / len, d.y / len };
   float half = thickness * 0.5f;
   float2 n = { -nd.y * half, nd.x * half };
 
   float4 c = { color.r(), color.g(), color.b(), 1.0f };
   DotLineVertex vertices[4] = {
    { { p1.x + n.x, p1.y + n.y }, c, 0.0f },
    { { p1.x - n.x, p1.y - n.y }, c, 0.0f },
    { { p2.x + n.x, p2.y + n.y }, c, len },
    { { p2.x - n.x, p2.y - n.y }, c, len },
   };
 
   auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
   pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 
   {
    void* pData = nullptr;
    pImmediateContext_->MapBuffer(m_draw_dot_line_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, vertices, sizeof(vertices));
    pImmediateContext_->UnmapBuffer(m_draw_dot_line_vertex_buffer, MAP_WRITE);
   }
 
   {
    auto viewportCB = viewport_.GetViewportCB();
    CBSolidTransform2D cbTransform;
    cbTransform.offset = viewportCB.offset;
    cbTransform.scale = viewportCB.scale;
    cbTransform.screenSize = viewportCB.screenSize;
 
    void* pData = nullptr;
    pImmediateContext_->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, &cbTransform, sizeof(cbTransform));
    pImmediateContext_->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
   }
 
   {
    struct DotLineShaderCB { float thickness; float spacing; float2 padding; };
    DotLineShaderCB cb = { thickness, spacing, {0,0} };
    void* pData = nullptr;
    pImmediateContext_->MapBuffer(m_draw_dot_line_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, &cb, sizeof(cb));
    pImmediateContext_->UnmapBuffer(m_draw_dot_line_cb, MAP_WRITE);
   }
 
   pImmediateContext_->SetPipelineState(m_draw_dot_line_pso_and_srb.pPSO);
 
   IBuffer* pBuffers[] = { m_draw_dot_line_vertex_buffer };
   Uint64 offsets[] = { 0 };
   pImmediateContext_->SetVertexBuffers(0, 1, pBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
 
   m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
   m_draw_dot_line_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "DotLineCB")->Set(m_draw_dot_line_cb);
   pImmediateContext_->CommitShaderResources(m_draw_dot_line_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 
   DrawAttribs drawAttrs;
   drawAttrs.NumVertices = 4;
   drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
   pImmediateContext_->Draw(drawAttrs);
  }
 
  void ArtifactIRenderer::Impl::drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color)
  {
   const int segments = 24;
   float2 lastPos = p0;
   for (int i = 1; i <= segments; ++i) {
    float t = (float)i / (float)segments;
    QPointF qp = BezierCalculator::evaluateQuadratic({ p0.x, p0.y }, { p1.x, p1.y }, { p2.x, p2.y }, t);
    float2 currentPos = { (float)qp.x(), (float)qp.y() };
    drawThickLineLocal(lastPos, currentPos, thickness, color);
    lastPos = currentPos;
   }
  }
 
  void ArtifactIRenderer::Impl::drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color)
  {
   const int segments = 32;
   float2 lastPos = p0;
   for (int i = 1; i <= segments; ++i) {
    float t = (float)i / (float)segments;
    QPointF qp = BezierCalculator::evaluateCubic({ p0.x, p0.y }, { p1.x, p1.y }, { p2.x, p2.y }, { p3.x, p3.y }, t);
    float2 currentPos = { (float)qp.x(), (float)qp.y() };
    drawThickLineLocal(lastPos, currentPos, thickness, color);
    lastPos = currentPos;
   }
  }
 
  void ArtifactIRenderer::Impl::drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color)
  {
   if (!pSwapChain_ || !m_draw_solid_triangle_pso_and_srb.pPSO) return;
 
   float4 c = { color.r(), color.g(), color.b(), 1.0f };
   RectVertex vertices[3] = {
    {{p0.x, p0.y}, c},
    {{p1.x, p1.y}, c},
    {{p2.x, p2.y}, c}
   };
 
   auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
   pImmediateContext_->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 
   {
    void* pData = nullptr;
    pImmediateContext_->MapBuffer(m_draw_solid_triangle_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, vertices, sizeof(vertices));
    pImmediateContext_->UnmapBuffer(m_draw_solid_triangle_vertex_buffer, MAP_WRITE);
   }
 
   {
    auto viewportCB = viewport_.GetViewportCB();
    CBSolidTransform2D cbTransform;
    cbTransform.offset = viewportCB.offset;
    cbTransform.scale = viewportCB.scale;
    cbTransform.screenSize = viewportCB.screenSize;
 
    void* pData = nullptr;
    pImmediateContext_->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
    std::memcpy(pData, &cbTransform, sizeof(cbTransform));
    pImmediateContext_->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
   }
 
   pImmediateContext_->SetPipelineState(m_draw_solid_triangle_pso_and_srb.pPSO);
 
   IBuffer* pBuffers[] = { m_draw_solid_triangle_vertex_buffer };
   Uint64 offsets[] = { 0 };
   pImmediateContext_->SetVertexBuffers(0, 1, pBuffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
 
   m_draw_solid_triangle_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
   pImmediateContext_->CommitShaderResources(m_draw_solid_triangle_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 
   DrawAttribs drawAttrs;
   drawAttrs.NumVertices = 3;
   drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
   pImmediateContext_->Draw(drawAttrs);
  }
 
  void ArtifactIRenderer::Impl::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
  {
    // Simply draw 4 lines
    float2 p1 = {x, y};
    float2 p2 = {x + w, y};
    float2 p3 = {x + w, y + h};
    float2 p4 = {x, y + h};
    
    drawLineLocal(p1, p2, color, color);
    drawLineLocal(p2, p3, color, color);
    drawLineLocal(p3, p4, color, color);
    drawLineLocal(p4, p1, color, color);
  }

 void ArtifactIRenderer::Impl::drawSpriteLocal(float x, float y, float w, float h, const QImage& image)
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
  float2 p1_ndc = viewport_.CanvasToNDC({x, y});
  float2 p2_ndc = viewport_.CanvasToNDC({x + w, y + h});
  float left = p1_ndc.x;
  float right = p2_ndc.x;
  float top = p1_ndc.y;
  float bottom = p2_ndc.y;

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

 ArtifactIRenderer::ArtifactIRenderer(RefCntAutoPtr<IRenderDevice> pDevice, RefCntAutoPtr<IDeviceContext> pImmediateContext, QWidget* widget) :impl_(new Impl(pDevice, pImmediateContext,widget))
 {

 }

 ArtifactIRenderer::ArtifactIRenderer():impl_(new Impl())
 {

 }

 ArtifactIRenderer::~ArtifactIRenderer()
 {
  delete impl_;
 }

 void ArtifactIRenderer::initialize(QWidget* widget)
 {
  impl_->initialize(widget);
 }

 void ArtifactIRenderer::createSwapChain(QWidget* widget)
 {
  impl_->createSwapChain(widget);
 }
 void ArtifactIRenderer::recreateSwapChain(QWidget* widget)
 {
  impl_->recreateSwapChain(widget);
 }

 void ArtifactIRenderer::clear()
 {
  impl_->clear();
 }

 void ArtifactIRenderer::flush()
 {
  if (!impl_->pImmediateContext_) return;
  impl_->pImmediateContext_->Flush();
 }

 void ArtifactIRenderer::flushAndWait()
 {
  impl_->flushAndWait();
 }

 void ArtifactIRenderer::beginFrameGpuProfiling()
 {
  impl_->beginFrameGpuProfiling();
 }

 void ArtifactIRenderer::endFrameGpuProfiling()
 {
  impl_->endFrameGpuProfiling();
 }

 double ArtifactIRenderer::lastFrameGpuTimeMs() const
 {
  return impl_->lastFrameGpuTimeMs();
 }

  void ArtifactIRenderer::drawSolidRect(float2 pos, float2 size, const FloatColor& color)
  {
    impl_->drawSolidRect(pos, size, color);
  }
 
  void ArtifactIRenderer::drawSolidRect(float x, float y, float w, float h)
  {
    impl_->drawSolidRect(x, y, w, h);
  }
 
  void ArtifactIRenderer::drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
  {
    impl_->drawRectOutlineLocal(x, y, w, h, color);
  }
 
  void ArtifactIRenderer::drawRectOutline(float2 pos, float2 size, const FloatColor& color)
  {
    impl_->drawRectOutlineLocal(pos.x, pos.y, size.x, size.y, color);
  }
 
  void ArtifactIRenderer::drawRectLocal(float x, float y, float w, float h, const FloatColor& color)
  {
   impl_->drawRectLocal(x, y, w, h, color);
  }
 
  void ArtifactIRenderer::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
  {
    impl_->drawRectOutlineLocal(x, y, w, h, color);
  }
 
  void ArtifactIRenderer::drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color)
  {
   impl_->drawThickLineLocal(p1, p2, thickness, color);
  }
 
  void ArtifactIRenderer::drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color)
  {
   impl_->drawDotLineLocal(p1, p2, thickness, spacing, color);
  }
 
  void ArtifactIRenderer::drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color)
  {
   impl_->drawBezierLocal(p0, p1, p2, thickness, color);
  }
 
  void ArtifactIRenderer::drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color)
  {
   impl_->drawBezierLocal(p0, p1, p2, p3, thickness, color);
  }
 
  void ArtifactIRenderer::drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color)
  {
   impl_->drawSolidTriangleLocal(p0, p1, p2, color);
  }
 
  void ArtifactIRenderer::present()
  {
   if (!impl_->pSwapChain_) return;
   impl_->pSwapChain_->Present();
  }
 
  void ArtifactIRenderer::setViewportSize(float w, float h) { impl_->setViewportSize(w, h); }
  void ArtifactIRenderer::setCanvasSize(float w, float h) { impl_->setCanvasSize(w, h); }
  void ArtifactIRenderer::setPan(float x, float y) { impl_->setPan(x, y); }
  void ArtifactIRenderer::setZoom(float zoom) { impl_->setZoom(zoom); }
  void ArtifactIRenderer::panBy(float dx, float dy) { impl_->panBy(dx, dy); }
  void ArtifactIRenderer::resetView() { impl_->resetView(); }
  void ArtifactIRenderer::fitToViewport(float margin) { impl_->fitToViewport(margin); }
  void ArtifactIRenderer::zoomAroundViewportPoint(float2 viewportPos, float newZoom) { impl_->zoomAroundViewportPoint(viewportPos, newZoom); }

  float2 ArtifactIRenderer::canvasToViewport(float2 pos) const { return impl_->canvasToViewport(pos); }
  float2 ArtifactIRenderer::viewportToCanvas(float2 pos) const { return impl_->viewportToCanvas(pos); }
 
  void ArtifactIRenderer::destroy()
  {
    impl_->destroy();
  }
 
};
