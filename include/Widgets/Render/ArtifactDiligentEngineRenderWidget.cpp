module;
#include <QWidget>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Common/interface/BasicMath.hpp>

module Widgets.Render.Composition;


namespace Artifact {

 using namespace Diligent;

 class ArtifactDiligentEngineComposition2DWindow::Impl {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;
  bool m_initialized = false;
 public:
  Impl();
  void initialize(QWindow*window);
  void recreateSwapChain();
  void clear();
 };

 void ArtifactDiligentEngineComposition2DWindow::Impl::initialize(QWindow*window)
{
  auto* pFactory = GetEngineFactoryD3D12();


  EngineD3D12CreateInfo CreationAttribs = {};
  CreationAttribs.EnableValidation = true;

  // ウィンドウハンドルを設定
  Win32NativeWindow VkWindow;
  VkWindow.hWnd = reinterpret_cast<HWND>(window->winId());
  pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice, &pImmediateContext);

  // スワップチェインを作成
  SwapChainDesc SCDesc;

  FullScreenModeDesc desc;

  desc.Fullscreen = false;

  pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, VkWindow, &pSwapChain);


  m_initialized = true;

 }

 void ArtifactDiligentEngineComposition2DWindow::Impl::clear()
 {

 }

 ArtifactDiligentEngineComposition2DWindow::Impl::Impl()
 {

 }
 ArtifactDiligentEngineComposition2DWindow::ArtifactDiligentEngineComposition2DWindow(QWindow* parent /*= nullptr*/)
 {

 }

 ArtifactDiligentEngineComposition2DWindow::~ArtifactDiligentEngineComposition2DWindow()
 {

 }


 ArtifactDiligentEngineComposition2DWidget::ArtifactDiligentEngineComposition2DWidget(QWidget* parent /*= nullptr*/) :QWidget(parent)
 {

 }

 

 void ArtifactDiligentEngineComposition2DWindow::resizeEvent(QResizeEvent*)
 {
 

 }

 bool ArtifactDiligentEngineComposition2DWindow::clear(const Diligent::float4& clearColor)
 {

  return true;
 }

 ArtifactDiligentEngineComposition2DWidget::~ArtifactDiligentEngineComposition2DWidget()
 {

 }

};