module;
#include <windows.h>
#include <d3d12.h>
//#include <d3>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
//#include <DiligentCore/Common/interface/map>

#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/DeviceContextD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentTools/Imgui/interface/ImGuiDiligentRenderer.hpp>
#include <QTimer>
#include <QDebug>
#include <wobjectimpl.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>

#include <QKeyEvent>

module Artifact.Widgets.Render.Layer;

import Graphics;
import Graphics.Shader.Set;
import Graphics.CBuffer.Constants;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;



namespace Artifact {
 using namespace Diligent;
 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactLayerEditor2DWidget)

  class ArtifactLayerEditor2DWidget::Impl
 {
 private:
  QWidget* widget_ = nullptr;
  bool m_initialized = false;
  void loadPSOCacheFromFile();
  void savePSOCache();
  void createShaders();
  void createConstBuffer();
  void createPSOs();
  void createBlendPSOs();

  RenderShaderPair m_draw_line_shaders;
  RenderShaderPair m_draw_solid_shaders;
  RenderShaderPair m_draw_sprit_shaders;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_trnsform_cb;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_cb;

  PSOAndSRB m_draw_solid_pso_and_srb;
  PSOAndSRB m_draw_sprite_pso_and_srb;
  RefCntAutoPtr<IBuffer> m_draw_solid_rect_vertex_buffer;
  RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;

  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;

  std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> m_blendShaderMap;
  //std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> m_blendPSOMap;

  std::map<LAYER_BLEND_TYPE, PSOAndSRB> m_BlendMap;

  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<IFence> m_layer_fence;
  ZoomScale2D zoom_;
  QPointF point_;
  bool hasDirectDraw = false;
 public:
  Impl();
  ~Impl();
  void initialize(QWidget* window);
  void destroy();
  void initializeDirectDraw();
  void initializeImGui(QWidget* window);
  void recreateSwapChain(QWidget* window);
  void clearCanvas(const Diligent::float4& clearColor);
  void renderOneFrame();
  void present();
  void drawLine(int x1, int y1, int x2, int y2, const FloatColor& color);
  void drawRect(float x, float y, float w, float h, const FloatColor& color);
  void drawRectOutline(float x, float y, float w, float h, float thick, const FloatColor& color);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(float x, float y, float w, float h, const QImage& image);
  void drawRectLocal(float x, float y, float w, float h,const FloatColor&color);
  void drawSpriteLocal(const QImage& image);

  void setClearColor(const FloatColor& color);
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain_;
  int m_CurrentPhysicalWidth = 0;
  int m_CurrentPhysicalHeight = 0;
  float m_CurrentDevicePixelRatio;
  QImage takeBackBuffer() const;

  void defaultHandleKeyPressEvent(QKeyEvent* event);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
 };

 ArtifactLayerEditor2DWidget::Impl::Impl()
 {
  point_.setX(0.5);
  point_.setY(0.5f);

 }

 ArtifactLayerEditor2DWidget::Impl::~Impl()
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::initialize(QWidget* window)
 {
  widget_ = window;
  //view_ = CreateInitialViewMatrix();

  auto* pFactory = GetEngineFactoryD3D12();

  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;
  // ウィンドウハンドルを設定
  Win32NativeWindow hWindow;
  hWindow.hWnd = reinterpret_cast<HWND>(window->winId());
  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice, &pImmediateContext);

  if (!pDevice)
  {
   // エラーログ出力、アプリケーション終了などの処理
   qWarning() << "Failed to create Diligent Engine device and contexts.";
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

  pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, hWindow, &pSwapChain_);

  Diligent::Viewport VP;
  VP.Width = static_cast<float>(m_CurrentPhysicalWidth);
  VP.Height = static_cast<float>(m_CurrentPhysicalHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  // SetViewportsの最後の2引数は、レンダーターゲットの物理ピクセルサイズを渡すのが安全
  pImmediateContext->SetViewports(1, &VP, m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);


  TextureDesc TexDesc;
  TexDesc.Name = "LayerRenderTarget";
  TexDesc.Type = RESOURCE_DIM_TEX_2D;
  TexDesc.Width = m_CurrentPhysicalWidth;
  TexDesc.Height = m_CurrentPhysicalHeight;
  TexDesc.MipLevels = 1;
  TexDesc.Format = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

  pDevice->CreateTexture(TexDesc, nullptr, &m_layerRT);


  initializeDirectDraw();

  createShaders();
  createPSOs();
  createConstBuffer();

  m_initialized = true;
 }

 void ArtifactLayerEditor2DWidget::Impl::initializeImGui(QWidget* window)
 {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  (void)io;
 }

 void ArtifactLayerEditor2DWidget::Impl::recreateSwapChain(QWidget* window)
 {
  if (!window || !pDevice)
  {

   return;
  }


  //pSwapChain->Release();
  const int newWidth = static_cast<int>(window->width() * window->devicePixelRatio());
  const int newHeight = static_cast<int>(window->height() * window->devicePixelRatio());
  const float newDevicePixelRatio = window->devicePixelRatio();
  qDebug() << "Impl::recreateSwapChain - Logical:" << window->width() << "x" << window->height()
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
  pImmediateContext->SetViewports(1, &VP, newWidth, newHeight);

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

  pDevice->CreateTexture(TexDesc, nullptr, &m_layerRT);

 }
 void ArtifactLayerEditor2DWidget::Impl::present()
 {
  if (pSwapChain_)
  {

   pSwapChain_->Present(1);
  }
 }

 void ArtifactLayerEditor2DWidget::Impl::renderOneFrame()
 {
  if (!pSwapChain_) return;

  const Diligent::float4& clearColor = { 0.5f,0.5f,0.5f,1.0f };
  clearCanvas(clearColor);

  FloatColor color = FloatColor();



  drawRect(0, 0, 600, 600, color);

  present();
 }

 void ArtifactLayerEditor2DWidget::Impl::loadPSOCacheFromFile()
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::savePSOCache()
 {

 }


 void ArtifactLayerEditor2DWidget::Impl::createShaders()
 {
  ShaderCreateInfo lineVsInfo;

  lineVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  lineVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  lineVsInfo.EntryPoint = "main";
  lineVsInfo.Desc.Name = "MyVertexShader";
  lineVsInfo.Source = lineShaderVSText.constData();
  lineVsInfo.SourceLength = lineShaderVSText.length();

  pDevice->CreateShader(lineVsInfo, &m_draw_line_shaders.VS);

  ShaderCreateInfo linePsInfo;

  linePsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  linePsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  linePsInfo.Desc.Name = "MyPixelShader";
  linePsInfo.Source = g_qsSolidColorPS2.constData();
  lineVsInfo.SourceLength = g_qsSolidColorPS2.length();

  pDevice->CreateShader(linePsInfo, &m_draw_line_shaders.PS);

  ShaderCreateInfo solidVsInfo;

  solidVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  solidVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  solidVsInfo.Desc.Name = "SolidVertexShader";
  solidVsInfo.Source = drawSolidRectVSSource.constData();
  solidVsInfo.SourceLength = drawSolidRectVSSource.length();

  pDevice->CreateShader(solidVsInfo, &m_draw_solid_shaders.VS);

  ShaderCreateInfo solidPsInfo;

  solidPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  solidPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  solidPsInfo.Desc.Name = "MyPixelShader";
  solidPsInfo.Source = g_qsSolidColorPSSource.constData();
  solidPsInfo.SourceLength = g_qsSolidColorPSSource.length();

  pDevice->CreateShader(solidPsInfo, &m_draw_solid_shaders.PS);

  ShaderCreateInfo sprite2DVsInfo;
  sprite2DVsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DVsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
  sprite2DVsInfo.Desc.Name = "SpriteShader";

  sprite2DVsInfo.Source = g_qsBasic2DVS.constData();
  sprite2DVsInfo.SourceLength = g_qsBasic2DVS.length();

  pDevice->CreateShader(sprite2DVsInfo, &m_draw_sprit_shaders.VS);

  ShaderCreateInfo sprite2DPsInfo;
  sprite2DPsInfo.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  sprite2DPsInfo.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
  sprite2DPsInfo.Desc.Name = "SpriteShader";

  sprite2DPsInfo.Source = g_qsBasicSprite2DImagePS.constData();
  sprite2DPsInfo.SourceLength = g_qsBasicSprite2DImagePS.length();

  pDevice->CreateShader(sprite2DPsInfo, &m_draw_sprit_shaders.PS);


 }

 void ArtifactLayerEditor2DWidget::Impl::createPSOs()
 {
  GraphicsPipelineStateCreateInfo drawLinePSOCreateInfo;

  // PSO名
  drawLinePSOCreateInfo.PSODesc.Name = "DrawSprite2D_PSO";

  drawLinePSOCreateInfo.pVS = m_draw_sprit_shaders.VS;
  drawLinePSOCreateInfo.pPS = m_draw_sprit_shaders.PS;
  //drawLinePSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
  //drawLinePSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

  GraphicsPipelineStateCreateInfo drawSolidRectPSOCreateInfo;
  drawSolidRectPSOCreateInfo.pVS = m_draw_solid_shaders.VS;
  drawSolidRectPSOCreateInfo.pPS = m_draw_solid_shaders.PS;
  drawSolidRectPSOCreateInfo.PSODesc.Name = "DrawSolidRectPSO";
  drawSolidRectPSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  // グラフィックスパイプライン設定
  auto& GP = drawSolidRectPSOCreateInfo.GraphicsPipeline;
  GP.NumRenderTargets = 1;
  GP.RTVFormats[0] = MAIN_RTV_FORMAT; // 出力先RTVのフォーマットに合わせる
  GP.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; // 矩形描画用
  GP.RasterizerDesc.CullMode = CULL_MODE_NONE;
  GP.DepthStencilDesc.DepthEnable = False;
  LayoutElement LayoutElems2[] =
  {
	  LayoutElement{0, 0, 2, VT_FLOAT32, false} // pos: float2
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

  pDevice->CreateGraphicsPipelineState(drawSolidRectPSOCreateInfo, &m_draw_solid_pso_and_srb.pPSO);

  m_draw_solid_pso_and_srb.pPSO->CreateShaderResourceBinding(&m_draw_solid_pso_and_srb.pSRB, false);

  //

  GraphicsPipelineStateCreateInfo drawSpritePSOCreateInfo;
  drawSpritePSOCreateInfo.PSODesc.Name = "DrawSprite PSO";
  drawSpritePSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
  ShaderResourceVariableDesc Vars[] =
  {
	  { SHADER_TYPE_PIXEL, "SolidColorCB", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC }
  };

  drawSpritePSOCreateInfo.pVS = m_draw_sprit_shaders.VS;
  drawSpritePSOCreateInfo.pPS = m_draw_sprit_shaders.PS;


  LayoutElement LayoutElems[] =
  {
	  LayoutElement{0, 0, 2, VT_FLOAT32, False},  // position : float2 → オフセット 0
	  LayoutElement{1, 0, 4, VT_FLOAT32, False}   // color    : float4 → オフセット 8
  };
  drawSpritePSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
  drawSpritePSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);
  drawSpritePSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  drawSpritePSOCreateInfo.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM_SRGB;
  drawSpritePSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_D24_UNORM_S8_UINT;

  // ブレンドステート（αブレンドON）
  auto& BSDesc = drawSpritePSOCreateInfo.GraphicsPipeline.BlendDesc;
  BSDesc.RenderTargets[0].BlendEnable = true;
  BSDesc.RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
  BSDesc.RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
  BSDesc.RenderTargets[0].BlendOp = BLEND_OPERATION_ADD;
  BSDesc.RenderTargets[0].RenderTargetWriteMask = COLOR_MASK_ALL;

  // ラスタライザ・深度
  drawSpritePSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
  drawSpritePSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = false;

  // デフォルトのサンプル設定
  //PSOCreateInfo.GraphicsPipeline.SampleDesc.Count = 1;


  RefCntAutoPtr<IPipelineStateCache> m_PSOCache;

  PipelineStateCacheCreateInfo CacheCI;
  CacheCI.Desc.Name = "SpritePSOCache";
  pDevice->CreatePipelineStateCache(CacheCI, &m_PSOCache);
  drawSpritePSOCreateInfo.pPSOCache = m_PSOCache;
  // PSO作成
 // pDevice->CreateGraphicsPipelineState(drawSpritePSOCreateInfo, &m_draw_sprite_pso_and_srb.pPSO);


 }

 void ArtifactLayerEditor2DWidget::Impl::createBlendPSOs()
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::createConstBuffer()
 {
  BufferDesc vbDesc;
  vbDesc.Name = "Sprite VB";
  //vbDesc.Size = sizeof(vertices);
  vbDesc.Usage = USAGE_DYNAMIC;
  vbDesc.BindFlags = BIND_VERTEX_BUFFER;
  BufferData vbData;
  //vbData.pData = vertices;
  //vbData.DataSize = sizeof(vertices);
  //device->CreateBuffer(vbDesc, &vbData, &vertexBuffer);
  {
   Diligent::BufferDesc CBDesc;
   CBDesc.Name = "DrawSolidColorCB";
   CBDesc.Usage = Diligent::USAGE_DYNAMIC;        // 動的に更新する場合
   CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER; // 定数バッファ
   CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;   // CPU側から書き込み可能
   CBDesc.Size = sizeof(CBSolidColor);


   pDevice->CreateBuffer(CBDesc, nullptr, &m_draw_solid_rect_cb);

  }
  {
   Diligent::BufferDesc CBDesc;
   CBDesc.Name = "DrawSolidTransformCB";
   CBDesc.Usage = Diligent::USAGE_DYNAMIC;        // 動的に更新する場合
   CBDesc.BindFlags = Diligent::BIND_UNIFORM_BUFFER; // 定数バッファ
   CBDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;   // CPU側から書き込み可能
   CBDesc.Size = sizeof(CBSolidTransform);


   pDevice->CreateBuffer(CBDesc, nullptr, &m_draw_solid_rect_trnsform_cb);

  }

  {
   BufferDesc vbDesc;
   vbDesc.Name = "SolidRect Vertex Buffer";
   vbDesc.BindFlags = BIND_VERTEX_BUFFER;
   vbDesc.Usage = USAGE_DYNAMIC;         // 頂点を毎フレーム更新する
   vbDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
   vbDesc.Size = sizeof(RectVertex) * 4; // 矩形4頂点ぶん

   // 初期データをセットする場合はここで指定（今は空なのでnullptrでOK）
   pDevice->CreateBuffer(vbDesc, nullptr, &m_draw_solid_rect_vertex_buffer);
  }
 }

 void ArtifactLayerEditor2DWidget::Impl::setClearColor(const FloatColor& color)
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::clearCanvas(const Diligent::float4& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

  if (!pSwapChain_) return;

  auto pRTV = pSwapChain_->GetCurrentBackBufferRTV();
  auto pDSV = pSwapChain_->GetDepthBufferDSV(); // 2Dならnullptrの場合が多い

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = { clearColor.r, clearColor.g,clearColor.b,clearColor.a };
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);






  if (pDSV) // pDSVがnullptrでないことを確認
  {
   pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }

 }
 void ArtifactLayerEditor2DWidget::Impl::drawSprite(float x, float y, float w, float h)
 {
  Vertex vertices[] =
  {
	  {{x,     y},     {0.0f, 0.0f}},
	  {{x + w, y},     {1.0f, 0.0f}},
	  {{x,     y + h}, {0.0f, 1.0f}},
	  {{x + w, y + h}, {1.0f, 1.0f}},
  };

  uint32_t indices[] = { 0, 1, 2, 2, 1, 3 };

  Uint32 offset = 0;

  pImmediateContext->SetPipelineState(m_draw_sprite_pso_and_srb.pPSO);
  pImmediateContext->CommitShaderResources(m_draw_sprite_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawAttribs attrs;
  attrs.NumVertices = 4;
  attrs.Flags = DRAW_FLAG_VERIFY_ALL;
  //pImmediateContext->DrawIndexed(sizeof(indices) / sizeof(indices[0]), 0, 0);
 }

 void ArtifactLayerEditor2DWidget::Impl::drawSprite(float x, float y, float w, float h, const QImage& image)
 {
  RectVertex vertices[4] = {
	{{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	{{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	{{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	{{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  ITextureView* RTVs[] = { m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) };
  pImmediateContext->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 }

 void ArtifactLayerEditor2DWidget::Impl::drawRect(float x, float y, float w, float h, const FloatColor& color)
 {
  RectVertex vertices[4] = {
	  {{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	  {{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	  {{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	  {{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  ITextureView* RTVs[] = { m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) };
  pImmediateContext->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  //uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };
  {
   void* pData = nullptr;
   pImmediateContext->MapBuffer(m_draw_solid_rect_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, vertices, sizeof(vertices));
   pImmediateContext->UnmapBuffer(m_draw_solid_rect_vertex_buffer, MAP_WRITE);
  }
  {
   CBSolidColor cb = { {1.0f, 0.0f, 0.0f, 1.0f} };


   void* pData = nullptr;
   pImmediateContext->MapBuffer(m_draw_solid_rect_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, &cb, sizeof(cb));
   pImmediateContext->UnmapBuffer(m_draw_solid_rect_cb, MAP_WRITE);
  }

  {
   CBSolidTransform cbTransform;
   cbTransform.offset = { x, y };
   cbTransform.scale = { w, h };


   void* pData = nullptr;
   pImmediateContext->MapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE, MAP_FLAG_DISCARD, pData);
   std::memcpy(pData, &cbTransform, sizeof(cbTransform));
   pImmediateContext->UnmapBuffer(m_draw_solid_rect_trnsform_cb, MAP_WRITE);
  }



  pImmediateContext->SetPipelineState(m_draw_solid_pso_and_srb.pPSO);

  IBuffer* pBuffers[] = { m_draw_solid_rect_vertex_buffer };
  Uint64 offsets[] = { 0 }; // Uint64 にする
  pImmediateContext->SetVertexBuffers(
   0,                  // 開始スロット
   1,                  // バッファ数
   pBuffers,           // バッファ配列
   offsets,            // オフセット配列（Uint64）
   RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
   SET_VERTEX_BUFFERS_FLAG_RESET
  );



  m_draw_solid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "TransformCB")->Set(m_draw_solid_rect_trnsform_cb);
  m_draw_solid_pso_and_srb.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "ColorBuffer")->Set(m_draw_solid_rect_cb);
  pImmediateContext->CommitShaderResources(m_draw_solid_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 4;
  drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
  pImmediateContext->Draw(drawAttrs);


 }

 void ArtifactLayerEditor2DWidget::Impl::drawRectOutline(float x, float y, float w, float h, float thick, const FloatColor& color)
 {
  RectVertex vertices[4] = {
	{{0.0f, 0.0f}, {1, 0, 0, 1}}, // 左上
	{{1.0f, 0.0f}, {1, 0, 0, 1}}, // 右上
	{{0.0f, 1.0f}, {1, 0, 0, 1}}, // 左下
	{{1.0f, 1.0f}, {1, 0, 0, 1}}, // 右下
  };

  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  ITextureView* RTVs[] = { m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) };
  pImmediateContext->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

 	
 	
 }

 void ArtifactLayerEditor2DWidget::Impl::drawLine(int x1, int y1, int x2, int y2, const FloatColor& color)
 {
  LineVertex vertex[2];  // これで長さ2の配列を作る

  vertex[0].position = float2((float)x1, (float)y1);
  vertex[0].color = float4(1, 1, 1, 1);  // 色も設定

  vertex[1].position = float2((float)x2, (float)y2);
  vertex[1].color = float4(1, 1, 1, 1);
  //vertex[0].color = color.toFloat4();
  //vertex[1].color = color.toFloat4();
  auto swapChainRTV = pSwapChain_->GetCurrentBackBufferRTV();
  ITextureView* RTVs[] = { m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) };
  pImmediateContext->SetRenderTargets(1, &swapChainRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


  //pImmediateContext->SetPipelineState(mdraw);
  //pImmediateContext->CommitShaderResources(m_draw_solid_pso_and_srb.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 	DrawAttribs drawAttrs;
  drawAttrs.NumVertices = 2;
  drawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
  //drawAttrs.Topology = PRIMITIVE_TOPOLOGY_LINE_LIST;
 }

 QImage ArtifactLayerEditor2DWidget::Impl::takeBackBuffer() const
 {
  auto backBuffer = pSwapChain_->GetCurrentBackBufferRTV();
  
 	
  TextureDesc desc;
  desc.Type = RESOURCE_DIM_TEX_2D;
  //desc.Width = width;
  //desc.Height = height;
  //desc.Format = backBuffer->GetDesc().Format;
  desc.Usage = USAGE_STAGING;              // CPU読み取り用
  desc.BindFlags = BIND_NONE;
  desc.CPUAccessFlags = CPU_ACCESS_READ;

  RefCntAutoPtr<ITexture> staging;
  pDevice->CreateTexture(desc, nullptr, &staging);

  RefCntAutoPtr<ITexture> pReadableTex;
  //pDevice->CreateTexture(ReadableDesc, nullptr, &pReadableTex);

 	

  return QImage();
 }

 void ArtifactLayerEditor2DWidget::Impl::drawSpriteLocal(const QImage& image)
 {

 	
 }

 void ArtifactLayerEditor2DWidget::Impl::initializeDirectDraw()
 {
  if (pDevice)
  {
   RefCntAutoPtr<IRenderDeviceD3D12> pDeviceD3D12;

   pDevice->QueryInterface(
	Diligent::IID_RenderDeviceD3D12,
	reinterpret_cast<Diligent::IObject**>(pDeviceD3D12.RawDblPtr())
   );


  	if (pDeviceD3D12)
  	{

  		
  	}



  }



 }

 void ArtifactLayerEditor2DWidget::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::defaultHandleKeyReleaseEvent(QKeyEvent* event)
 {

 }

 void ArtifactLayerEditor2DWidget::Impl::destroy()
 {
  if (pSwapChain_)
  {
   // GPUリソースを安全に破棄
   pSwapChain_.Release();
   //pSwapChain_ = nullptr;
  }

  // もし DeviceContext や RenderDevice も持っているなら
  
  
  //pDeviceContext_.Release();
  //pRenderDevice_.Release();
 }

 ArtifactLayerEditor2DWidget::ArtifactLayerEditor2DWidget(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->initialize(this);
  QTimer* renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
   impl_->renderOneFrame();

   });
  renderTimer->start(16);

  connect(this, &QWidget::destroyed, renderTimer, &QTimer::stop);

  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
 }

 ArtifactLayerEditor2DWidget::~ArtifactLayerEditor2DWidget()
 {
  impl_->destroy();

  delete impl_;
 }

 void ArtifactLayerEditor2DWidget::mousePressEvent(QMouseEvent* event)
 {

  //impl_->defaultHandleKeyPressEvent(event);

 }

 void ArtifactLayerEditor2DWidget::mouseReleaseEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactLayerEditor2DWidget::mouseDoubleClickEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactLayerEditor2DWidget::mouseMoveEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactLayerEditor2DWidget::wheelEvent(QWheelEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactLayerEditor2DWidget::setEditMode(EditMode mode)
 {

 }

 void ArtifactLayerEditor2DWidget::setDisplayMode(DisplayMode mode)
 {

 }

 void ArtifactLayerEditor2DWidget::setTargetLayerId(int id)
 {

 }

 void ArtifactLayerEditor2DWidget::resetView()
 {

 }

 void ArtifactLayerEditor2DWidget::ChangeRenderAPI()
 {

 }

 void ArtifactLayerEditor2DWidget::setClearColor(const FloatColor& color)
 {

 }

 void ArtifactLayerEditor2DWidget::setZoom(const ZoomScale2D& scale)
 {

 }

 void ArtifactLayerEditor2DWidget::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  impl_->recreateSwapChain(this);
 }

 void ArtifactLayerEditor2DWidget::paintEvent(QPaintEvent* event)
 {
  impl_->renderOneFrame();

 }

 void ArtifactLayerEditor2DWidget::keyPressEvent(QKeyEvent* event)
 {
  switch (event->key())
  {

  case Qt::Key_V:  // AE: Selection tool
   //currentTool = ToolType::Select;
   break;
  case Qt::Key_H:  // Hand tool
   //currentTool = ToolType::Hand;
   break;
  case Qt::Key_Z:  // Zoom tool
   //currentTool = ToolType::Zoom;
   break;
  case Qt::Key_Space: // Temporarily switch to hand tool
   //tempTool = ToolType::Hand;
   break;
  default:
   QWidget::keyPressEvent(event);
   return;
  }
 }

 void ArtifactLayerEditor2DWidget::keyReleaseEvent(QKeyEvent* event)
 {



 }

 void ArtifactLayerEditor2DWidget::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();

  QWidget::closeEvent(event);
 }

};