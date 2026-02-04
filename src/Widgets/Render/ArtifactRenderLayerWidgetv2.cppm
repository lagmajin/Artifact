module;
#define NOMINMAX
#include <windows.h>
#include <tbb/tbb.h>
#include <QList>
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


#include <wobjectimpl.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <QTimer>
#include <QDebug>
#include <QKeyEvent>

module Artifact.Widgets.RenderLayerWidgetv2;
import Graphics;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Artifact.Application.Manager;
import Artifact.Service.Application;
import Artifact.Service.Project;
import Artifact.Service.ActiveContext;

import Artifact.Render.IRenderer;
import Artifact.Preview.Pipeline;

namespace Artifact {

W_OBJECT_IMPL(ArtifactLayerEditorWidgetV2)

 class ArtifactLayerEditorWidgetV2::Impl {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
 public:
  Impl();
  ~Impl();
  void initialize(QWidget* window);
  void initializeSwapChain(QWidget* window);
  void destroy();
  std::unique_ptr<AritfactIRenderer> renderer_;
  bool isPanning_=false;
  QPointF lastMousePos_;
  QWidget* widget_;
  //bool isPanning_ = false;
  bool isPlay_ = false;
  std::atomic_bool running_{ false };
  tbb::task_group renderTask_;
  std::mutex resizeMutex_;
  
  
  bool released = true;
  bool m_initialized;
  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<IFence> m_layer_fence;
  
  void defaultHandleKeyPressEvent(QKeyEvent* event);
  void defaultHandleKeyReleaseEvent(QKeyEvent* event);
  void recreateSwapChain(QWidget* window);
  void recreateSwapChainInternal(QWidget* window);
  
  void startRenderLoop();
  void stopRenderLoop();
  void renderOneFrame();
 };

 ArtifactLayerEditorWidgetV2::Impl::Impl()
 {

 }

 ArtifactLayerEditorWidgetV2::Impl::~Impl()
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::initialize(QWidget* window)
 {/*
  widget_ = window;
  //view_ = CreateInitialViewMatrix();

  auto* pFactory = GetEngineFactoryD3D12();

  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;
  CreationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
  CreationAttribs.EnableValidation = true;
   //CreationAttribs.Features
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
   
   */
  renderer_ = std::make_unique<AritfactIRenderer>();
  renderer_->initialize(window);
   



 }

 void ArtifactLayerEditorWidgetV2::Impl::initializeSwapChain(QWidget* window)
 {
  renderer_->recreateSwapChain(window);
 }

 void ArtifactLayerEditorWidgetV2::Impl::destroy()
 {
  renderer_->destroy();
  //pImmediateContext.Release();
  //pDevice.Release();
 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyPressEvent(QKeyEvent* event)
 {
 
 }

 void ArtifactLayerEditorWidgetV2::Impl::defaultHandleKeyReleaseEvent(QKeyEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChainInternal(QWidget* window)
 {

 }

 void ArtifactLayerEditorWidgetV2::Impl::startRenderLoop()
 {
  if (running_)
   return;
  running_ = true;

  renderTask_.run([this]()
   {
	while (running_.load(std::memory_order_acquire))
	{
	 renderOneFrame();
	 std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	// ループ終了時に GPU 完全停止
	//pDevice->WaitForIdle();
   });
 }

 void ArtifactLayerEditorWidgetV2::Impl::stopRenderLoop()
 {
  running_ = false;        // ループを抜ける

  renderTask_.wait();

  renderer_->flushAndWait();
 }

 void ArtifactLayerEditorWidgetV2::Impl::renderOneFrame()
 {
  renderer_->clear();
  renderer_->present();
 }

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChain(QWidget* window)
 {

 }

 ArtifactLayerEditorWidgetV2::ArtifactLayerEditorWidgetV2(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setMinimumSize(1, 1);

  impl_->initialize(this);
  impl_->initializeSwapChain(this);
  impl_->startRenderLoop();
   
  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  setWindowTitle("ArtifactLayerEditor");

 }

 ArtifactLayerEditorWidgetV2::~ArtifactLayerEditorWidgetV2()
 {
  impl_->stopRenderLoop();
  delete impl_;
 }

 void ArtifactLayerEditorWidgetV2::keyPressEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyPressEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::keyReleaseEvent(QKeyEvent* event)
 {
  impl_->defaultHandleKeyReleaseEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mousePressEvent(QMouseEvent* event)
 {
  if (event->button() == Qt::MiddleButton ||
   (event->button() == Qt::RightButton && event->modifiers() & Qt::AltModifier))
  {
   impl_->isPanning_ = true;
   impl_->lastMousePos_ = event->position(); // 前回位置を保存
   event->accept();
   return;
  }

  QWidget::mousePressEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::mouseReleaseEvent(QMouseEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::mouseDoubleClickEvent(QMouseEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::mouseMoveEvent(QMouseEvent* event)
 {

 }


 void ArtifactLayerEditorWidgetV2::wheelEvent(QWheelEvent* event)
 {
  const float zoomStep = 0.1f;
  float delta = event->angleDelta().y() / 120.0f;

  //impl_->zoom_ += delta * zoomStep;

 }

 void ArtifactLayerEditorWidgetV2::resizeEvent(QResizeEvent* event)
 {
  QWidget::resizeEvent(event);
  impl_->recreateSwapChain(this);

 }

 void ArtifactLayerEditorWidgetV2::paintEvent(QPaintEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::showEvent(QShowEvent* event)
 {
  QWidget::showEvent(event);
  //impl_->initialize(this);
 }
 void ArtifactLayerEditorWidgetV2::closeEvent(QCloseEvent* event)
 {
	 impl_->destroy();
 }

 void ArtifactLayerEditorWidgetV2::focusInEvent(QFocusEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::setClearColor(const FloatColor& color)
 {

 }

 void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
 {

 }

 void ArtifactLayerEditorWidgetV2::resetView()
 {

 }

 void ArtifactLayerEditorWidgetV2::setEditMode(EditMode mode)
 {

 }

 void ArtifactLayerEditorWidgetV2::setDisplayMode(DisplayMode mode)
 {

 }

 void ArtifactLayerEditorWidgetV2::setPan(const QPointF& offset)
 {

 }

};