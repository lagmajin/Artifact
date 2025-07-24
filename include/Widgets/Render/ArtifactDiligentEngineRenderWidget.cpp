module;
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> 
#include <QWidget>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>

#include <DiligentTools/Imgui/interface/ImGuiDiligentRenderer.hpp>

#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <QBoxLayout>
#include <QTimer>
#include <wobjectimpl.h>
#include "qevent.h"
//#include <algorithm>

module Widgets.Render.Composition;


namespace Artifact {

 using namespace Diligent;

 W_OBJECT_IMPL(ArtifactDiligentEngineComposition2DWindow)

 class ArtifactDiligentEngineComposition2DWindow::Impl {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
  bool m_initialized = false;
  int m_CurrentPhysicalWidth;
  int m_CurrentPhysicalHeight;
  qreal m_CurrentDevicePixelRatio;
  glm::mat4 calculateModelMatrixGLM(const glm::vec2& position,      // 画面上の最終的なピクセル位置
   const glm::vec2& size,          // レイヤーの元のピクセルサイズ
   const glm::vec2& anchorPoint,   // ローカル正規化座標 (0.0-1.0) でのアンカーポイント
   float rotationDegrees,          // 回転角度 (度数法)
   const glm::vec2& scale);
  void createShader();
 public:
  Impl();
  void initialize(QWidget*window);
  void initializeImGui(QWindow* window);
  void recreateSwapChain(QWidget* window);
  void clear(const Diligent::float4& clearColor);
  void present();
  void renderOneFrame();
  void drawTexturedQuad();
  void drawSolidQuad(float x,float y,float w,float h);
 };

 void ArtifactDiligentEngineComposition2DWindow::Impl::initialize(QWidget*window)
{
  auto* pFactory = GetEngineFactoryD3D12();


  EngineD3D12CreateInfo CreationAttribs = {};
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
  SCDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
  SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
  SCDesc.BufferCount = 2;
  SCDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;

  FullScreenModeDesc desc;

  desc.Fullscreen = false;

  pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, hWindow, &pSwapChain);
  
  Diligent::Viewport VP;
  VP.Width = static_cast<float>(m_CurrentPhysicalWidth);
  VP.Height = static_cast<float>(m_CurrentPhysicalHeight);
  VP.MinDepth = 0.0f;
  VP.MaxDepth = 1.0f;
  VP.TopLeftX = 0.0f;
  VP.TopLeftY = 0.0f;
  // SetViewportsの最後の2引数は、レンダーターゲットの物理ピクセルサイズを渡すのが安全
  pImmediateContext->SetViewports(1, &VP, m_CurrentPhysicalWidth, m_CurrentPhysicalHeight);



  GraphicsPipelineStateCreateInfo PSOCreateInfo;

  // Pipeline state name is used by the engine to report issues.
  // It is always a good idea to give objects descriptive names.
  PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

  auto& blendDesc=PSOCreateInfo.GraphicsPipeline.BlendDesc;

  blendDesc.RenderTargets[0].BlendEnable= Diligent::True;
  
 


  m_initialized = true;

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::clear(const Diligent::float4& clearColor)
 {
  if (!m_initialized)
  {
   return;
  }

  auto pRTV = pSwapChain->GetCurrentBackBufferRTV();
  auto pDSV = pSwapChain->GetDepthBufferDSV(); // 2Dならnullptrの場合が多い
 
  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float ClearColor[] = {clearColor.r, clearColor.g,clearColor.b,clearColor.a};
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  //pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pImmediateContext->Flush();
 }

 ArtifactDiligentEngineComposition2DWindow::Impl::Impl()
 {
  //initialize();
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::recreateSwapChain(QWidget* window)
 {
  if (!window ||!pDevice)
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
  qDebug() << "Before Resize - SwapChain Desc:" << pSwapChain->GetDesc().Width << "x" << pSwapChain->GetDesc().Height;
  pSwapChain->Resize(newWidth, newHeight);


  qDebug() << "After Resize - SwapChain Desc:" << pSwapChain->GetDesc().Width << "x" << pSwapChain->GetDesc().Height;

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

 void ArtifactDiligentEngineComposition2DWindow::Impl::createShader()
 {
  Diligent::ShaderCreateInfo ShaderCI;
  ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL; // または GLSL
  ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
 }

 glm::mat4 ArtifactDiligentEngineComposition2DWindow::Impl::calculateModelMatrixGLM(const glm::vec2& position, /* 画面上の最終的なピクセル位置 */ const glm::vec2& size, /* レイヤーの元のピクセルサイズ */ const glm::vec2& anchorPoint, /* ローカル正規化座標 (0.0-1.0) でのアンカーポイント */ float rotationDegrees, /* 回転角度 (度数法) */ const glm::vec2& scale)
 {
  float anchorX_px = anchorPoint.x * size.x;
  float anchorY_px = anchorPoint.y * size.y;

  // 2. 変換行列の構築
  glm::mat4 model = glm::mat4(1.0f); // 単位行列で初期化

  // 2a. アンカーポイントを原点に移動
  // GLM はデフォルトで左から右へ乗算 (行ベクトル * 行列)
  model = glm::translate(model, glm::vec3(-anchorX_px, -anchorY_px, 0.0f));

  // 2b. スケーリング
  model = glm::scale(model, glm::vec3(scale.x, scale.y, 1.0f));

  // 2c. 回転 (Z軸周り)
  float rotationRadians = glm::radians(rotationDegrees); // 度数法からラジアンに変換
  model = glm::rotate(model, rotationRadians, glm::vec3(0.0f, 0.0f, 1.0f));

  // 2d. アンカーポイントを元の位置に戻す + レイヤーの最終位置に移動
  model = glm::translate(model, glm::vec3(position.x + anchorX_px, position.y + anchorY_px, 0.0f));

  return model;
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::present()
 {
  if (pSwapChain)
  {
  
  pSwapChain->Present(1);
  }
 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::initializeImGui(QWindow* window)
 {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  (void)io;


 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::renderOneFrame()
 {
  const Diligent::float4& clearColor = { 0.2f,0.5f,0.5f,1.0f };
  clear(clearColor);


  present();
 }

 ArtifactDiligentEngineComposition2DWindow::ArtifactDiligentEngineComposition2DWindow(QWidget* parent /*= nullptr*/):QWidget(parent),impl_(new Impl())
 {
  impl_->initialize(this);

  QTimer* renderTimer = new QTimer(this);
  connect(renderTimer, &QTimer::timeout, this, [this]() {
   impl_->renderOneFrame();

        });
  renderTimer->start(16);

  setAttribute(Qt::WA_NativeWindow);

  // Setting these attributes to our widget and returning null on paintEngine event
  // tells Qt that we'll handle all drawing and updating the widget ourselves.
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);
 }

 ArtifactDiligentEngineComposition2DWindow::~ArtifactDiligentEngineComposition2DWindow()
 {
  delete impl_;
 }
 void ArtifactDiligentEngineComposition2DWindow::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  impl_->recreateSwapChain(this);

  //const Diligent::float4& clearColor = { 0.2f,0.5f,0.5f,1.0f };
  //impl_->clear(clearColor);

  //impl_->present();
 }

 bool ArtifactDiligentEngineComposition2DWindow::clear(const Diligent::float4& clearColor)
 {

  return true;
 }

 class  ArtifactDiligentEngineComposition2DWidget::Impl {
 private:

 public:
  ArtifactDiligentEngineComposition2DWindow* window_ = nullptr;
  Impl(QWidget* widget);

};

 W_OBJECT_IMPL(ArtifactDiligentEngineComposition2DWidget)

 ArtifactDiligentEngineComposition2DWidget::Impl::Impl(QWidget* widget)
 {
  //window_ = new ArtifactDiligentEngineComposition2DWindow();
  

  //QWidget* container = QWidget::createWindowContainer(window_,widget);
 // container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  //QVBoxLayout* layout = new QVBoxLayout();
  //layout->addWidget(container,1);
  //layout->setContentsMargins(0, 0, 0, 0); // マージンを0に設定
  //layout->setSpacing(0);
  //widget->setLayout(layout);
  //container->setMinimumSize(0, 0);
  //container->setAutoFillBackground(false);
  //container->setStyleSheet("background-color:red;");
  //container->setAttribute(Qt::WA_TranslucentBackground);
  //container->setAttribute(Qt::WA_NoSystemBackground);
 }

 ArtifactDiligentEngineComposition2DWidget::ArtifactDiligentEngineComposition2DWidget(QWidget* parent /*= nullptr*/) :QWidget(parent),impl_(new Impl(this))
 {
  setAttribute(Qt::WA_NoSystemBackground); // システム背景の描画を無効化
  setAttribute(Qt::WA_TranslucentBackground);
  //QVBoxLayout* mainLayout = new QVBoxLayout(this);


  //mainLayout->addWidget(m_compositionWidget, 1);
 }

 ArtifactDiligentEngineComposition2DWidget::~ArtifactDiligentEngineComposition2DWidget()
 {
  delete impl_;
 }

 void ArtifactDiligentEngineComposition2DWidget::paintEvent(QPaintEvent* event)
 {
  Q_UNUSED(event);
 }


};