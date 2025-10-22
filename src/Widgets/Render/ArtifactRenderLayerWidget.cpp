module;
#include <windows.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>

#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentTools/Imgui/interface/ImGuiDiligentRenderer.hpp>
#include <QDebug>
#include <wobjectimpl.h>
module Artifact.Widgets.Render.Layer;

import Graphics;
import Graphics.Shader.Set;
import Graphics.CBuffer.Constants;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;



namespace Artifact {
 using namespace Diligent;
 using namespace ArtifactCore;

 W_OBJECT_IMPL(ArtifactRenderLayerWidget)

 class ArtifactRenderLayerWidget::Impl
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

  PSOAndSRB m_draw_sprite_pso_and_srb;
  RefCntAutoPtr<IBuffer> m_draw_sprite_vertex_buffer;
  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;

  std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IShader>> m_blendShaderMap;
  //std::map<LAYER_BLEND_TYPE, RefCntAutoPtr<IPipelineState>> m_blendPSOMap;

  std::map<LAYER_BLEND_TYPE, PSOAndSRB> m_BlendMap;

  RefCntAutoPtr<ITexture> layerRT_;
 public:
  Impl();
  ~Impl();
  void initialize(QWidget* window);
  void initializeImGui(QWidget* window);
  void recreateSwapChain(QWidget* window);
  void clearCanvas(const Diligent::float4& clearColor);
  void renderOneFrame();
  void present();
  void drawRect(float x,float y,float w,float h,const FloatColor& color);
  void drawSprite(float x,float y,float w,float h);
  void setClearColor(const FloatColor& color);
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain_;
  int m_CurrentPhysicalWidth = 0;
  int m_CurrentPhysicalHeight = 0;
  float m_CurrentDevicePixelRatio;
 };

 ArtifactRenderLayerWidget::Impl::Impl()
 {

 }

 ArtifactRenderLayerWidget::Impl::~Impl()
 {

 }

 void ArtifactRenderLayerWidget::Impl::initialize(QWidget* window)
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
  createShaders();
  createPSOs();
 }

 void ArtifactRenderLayerWidget::Impl::initializeImGui(QWidget* window)
 {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  (void)io;
 }

 void ArtifactRenderLayerWidget::Impl::recreateSwapChain(QWidget* window)
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
 }
 void ArtifactRenderLayerWidget::Impl::present()
 {
  if (pSwapChain_)
  {

   pSwapChain_->Present(1);
  }
 }

 void ArtifactRenderLayerWidget::Impl::renderOneFrame()
 {
  const Diligent::float4& clearColor = { 0.5f,0.5f,0.5f,1.0f };
  //clearCanvas(clearColor);


  present();
 }

 void ArtifactRenderLayerWidget::Impl::loadPSOCacheFromFile()
 {

 }

 void ArtifactRenderLayerWidget::Impl::savePSOCache()
 {

 }


 void ArtifactRenderLayerWidget::Impl::createShaders()
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

 void ArtifactRenderLayerWidget::Impl::createPSOs()
 {
  GraphicsPipelineStateCreateInfo drawLinePSOCreateInfo;

  // PSO名
  drawLinePSOCreateInfo.PSODesc.Name = "DrawSprite2D_PSO";

  drawLinePSOCreateInfo.pVS = m_draw_sprit_shaders.VS;
  drawLinePSOCreateInfo.pPS = m_draw_sprit_shaders.PS;
  //drawLinePSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
  //drawLinePSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

 	
  GraphicsPipelineStateCreateInfo drawSpritePSOCreateInfo;

  // PSO名
  drawSpritePSOCreateInfo.PSODesc.Name = "DrawSprite2D_PSO";

  drawSpritePSOCreateInfo.pVS = m_draw_sprit_shaders.VS;
  drawSpritePSOCreateInfo.pPS = m_draw_sprit_shaders.PS;

  LayoutElement LayoutElems[] =
  {
      LayoutElement{0, 0, 2, VT_FLOAT32, false}, // POSITION (x, y)
      LayoutElement{1, 0, 2, VT_FLOAT32, false}, // TEXCOORD (u, v)
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
  pDevice->CreateGraphicsPipelineState(drawSpritePSOCreateInfo, &m_draw_sprite_pso_and_srb.pPSO);

 	
 }

 void ArtifactRenderLayerWidget::Impl::createBlendPSOs()
 {

 }

 void ArtifactRenderLayerWidget::Impl::drawSprite(float x, float y, float w, float h)
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
  pImmediateContext->CommitShaderResources(m_draw_sprite_pso_and_srb.pSRB,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawAttribs attrs;
  attrs.NumVertices = 4;
  attrs.Flags = DRAW_FLAG_VERIFY_ALL;
  //pImmediateContext->DrawIndexed(sizeof(indices) / sizeof(indices[0]), 0, 0);
 }

 void ArtifactRenderLayerWidget::Impl::createConstBuffer()
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
 }

 void ArtifactRenderLayerWidget::Impl::setClearColor(const FloatColor& color)
 {

 }

 void ArtifactRenderLayerWidget::Impl::clearCanvas(const Diligent::float4& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

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

 void ArtifactRenderLayerWidget::Impl::drawRect(float x, float y, float w, float h, const FloatColor& color)
 {
  RectVertex vertices[4] = {
    {{x,     y},     {1,0,0,1}}, // 左上
    {{x + w,   y},     {1,0,0,1}}, // 右上
    {{x,     y + h},   {1,0,0,1}}, // 左下
    {{x + w,   y + h},   {1,0,0,1}}, // 右下
  };

  uint32_t indices[6] = { 0, 1, 2, 2, 1, 3 };
 }

 ArtifactRenderLayerWidget::ArtifactRenderLayerWidget(QWidget* parent/*=nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  impl_->initialize(this);

 }

 ArtifactRenderLayerWidget::~ArtifactRenderLayerWidget()
 {
  delete impl_;
 }

 void ArtifactRenderLayerWidget::mousePressEvent(QMouseEvent* event)
 {


 	
 }

 void ArtifactRenderLayerWidget::mouseReleaseEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactRenderLayerWidget::mouseDoubleClickEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactRenderLayerWidget::mouseMoveEvent(QMouseEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactRenderLayerWidget::wheelEvent(QWheelEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void ArtifactRenderLayerWidget::setEditMode(EditMode mode)
 {

 }

 void ArtifactRenderLayerWidget::setDisplayMode(DisplayMode mode)
 {

 }

 void ArtifactRenderLayerWidget::setTargetLayerId(int id)
 {

 }

 void ArtifactRenderLayerWidget::resetView()
 {

 }

 void ArtifactRenderLayerWidget::ChangeRenderAPI()
 {

 }

 void ArtifactRenderLayerWidget::setClearColor(const FloatColor& color)
 {

 }

 void ArtifactRenderLayerWidget::setZoom(const ZoomScale2D& scale)
 {

 }

};