module;
#include <EngineFactory.h>
#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <PipelineState.h>
#include <wobjectimpl.h>
#include <windows.h>

#include <QSize>
#include <QEvent>


#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>

module ArtifactDiligentEngineRenderWindow;

import Graphics;

namespace {
 Diligent::IEngineFactoryD3D12* resolveD3D12Factory()
 {
#if D3D12_SUPPORTED
#if DILIGENT_D3D12_SHARED
  return Diligent::LoadAndGetEngineFactoryD3D12();
#else
  return Diligent::GetEngineFactoryD3D12();
#endif
#else
  return nullptr;
#endif
 }

 Diligent::IEngineFactoryVk* resolveVkFactory()
 {
#if VULKAN_SUPPORTED
#if DILIGENT_VK_EXPLICIT_LOAD
  return Diligent::LoadAndGetEngineFactoryVk();
#else
  return Diligent::GetEngineFactoryVk();
#endif
#else
  return nullptr;
#endif
 }
}

namespace Artifact {

 W_OBJECT_IMPL(ArtifactDiligentEngineRenderWindow)

 class ArtifactDiligentEngineRenderWindow::Impl
 {
 private:
  RefCntAutoPtr<IRenderDevice> pDevice;
  RefCntAutoPtr<IDeviceContext> pImmediateContext;
  RefCntAutoPtr<ISwapChain> pSwapChain;

 public:
  Impl();
  ~Impl();
  bool initialize();
  void clear();
  void drawGrid();
  void postProcessing();
 };

 ArtifactDiligentEngineRenderWindow::Impl::~Impl()
 {

 }

 bool ArtifactDiligentEngineRenderWindow::Impl::initialize()
 {
  return false;
 }

 void ArtifactDiligentEngineRenderWindow::Impl::drawGrid()
 {

 }


 ArtifactDiligentEngineRenderWindow::ArtifactDiligentEngineRenderWindow(QWindow* parent /*= nullptr*/) :QWindow(parent)
 {

 }

 ArtifactDiligentEngineRenderWindow::~ArtifactDiligentEngineRenderWindow()
 {

 }

 void ArtifactDiligentEngineRenderWindow::renderWireframeObject()
 {
  PipelineStateCreateInfo PSOCreateInfo;
  auto desc=PSOCreateInfo.PSODesc;
 }

 bool ArtifactDiligentEngineRenderWindow::initialize()
 {
  auto tryInitD3D12 = [&]() -> bool {
   auto* pFactory = resolveD3D12Factory();
   if (!pFactory) return false;
   EngineD3D12CreateInfo CreationAttribs = {};
   CreationAttribs.EnableValidation = true;
   Win32NativeWindow VkWindow;
   VkWindow.hWnd = reinterpret_cast<HWND>(winId());
   pFactory->CreateDeviceAndContextsD3D12(CreationAttribs, &pDevice, &pImmediateContext);
   if (!pDevice || !pImmediateContext) return false;
   SwapChainDesc SCDesc;
   FullScreenModeDesc desc;
   desc.Fullscreen = false;
   pFactory->CreateSwapChainD3D12(pDevice, pImmediateContext, SCDesc, desc, VkWindow, &pSwapChain);
   return pSwapChain != nullptr;
  };

  auto tryInitVk = [&]() -> bool {
   auto* pFactoryVk = resolveVkFactory();
   if (!pFactoryVk) return false;
   EngineVkCreateInfo CreationAttribs = {};
   CreationAttribs.EnableValidation = true;
   Win32NativeWindow VkWindow;
   VkWindow.hWnd = reinterpret_cast<HWND>(winId());
   pFactoryVk->CreateDeviceAndContextsVk(CreationAttribs, &pDevice, &pImmediateContext);
   if (!pDevice || !pImmediateContext) return false;
   SwapChainDesc SCDesc;
   FullScreenModeDesc desc;
   desc.Fullscreen = false;
   pFactoryVk->CreateSwapChainVk(pDevice, pImmediateContext, SCDesc, VkWindow, &pSwapChain);
   return pSwapChain != nullptr;
  };

  QString backendStr = qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").toLower();
  bool initSuccess = false;

  if (backendStr == "vulkan" || backendStr == "vk") {
      initSuccess = tryInitVk() || tryInitD3D12();
  } else if (backendStr == "software" || backendStr == "sw") {
      initSuccess = false; // Force software
  } else {
      initSuccess = tryInitD3D12();
  }

  if (!initSuccess) {
      useSoftwareFallback_ = true;
  }
  
  m_initialized = true;

  return true;
 }

 void ArtifactDiligentEngineRenderWindow::setShadingMode(ShadingMode mode)
 {
  shadingMode_ = mode;
  requestRender();
 }

 ArtifactDiligentEngineRenderWindow::ShadingMode ArtifactDiligentEngineRenderWindow::shadingMode() const
 {
  return shadingMode_;
 }

 void ArtifactDiligentEngineRenderWindow::setClearColor(const QColor& color)
 {
  clearColor_ = color;
  requestRender();
 }

 QColor ArtifactDiligentEngineRenderWindow::clearColor() const
 {
  return clearColor_;
 }

 void ArtifactDiligentEngineRenderWindow::requestRender()
 {
  if (!isExposed())
   return;
  if (!m_initialized)
   initialize();
  render();
  present();
 }

 void ArtifactDiligentEngineRenderWindow::pickingRay(int posx, int posy)
 {

 }

 void ArtifactDiligentEngineRenderWindow::render()
 {
  if (!m_initialized)
   return;

  if (useSoftwareFallback_) {
      return; // fallback placeholder
  }

  if (!pSwapChain || !pImmediateContext) return;

  auto* pRTV = pSwapChain->GetCurrentBackBufferRTV();
  auto* pDSV = pSwapChain->GetDepthBufferDSV();

  pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  float r = static_cast<float>(clearColor_.redF());
  float g = static_cast<float>(clearColor_.greenF());
  float b = static_cast<float>(clearColor_.blueF());
  if (shadingMode_ == ShadingMode::Wireframe) {
   r = (std::min)(1.0f, r + 0.10f);
   g = (std::min)(1.0f, g + 0.10f);
   b = (std::min)(1.0f, b + 0.10f);
  } else if (shadingMode_ == ShadingMode::SolidWithWire) {
   b = (std::min)(1.0f, b + 0.12f);
  }
  const float ClearColor[] = { r, g, b, 1.0f };
  // Let the engine perform required state transitions
  pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 }

 void ArtifactDiligentEngineRenderWindow::present()
 {
  if (useSoftwareFallback_) return;

  if (pSwapChain) {
      pSwapChain->Present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::resizeEvent(QResizeEvent* event)
 {
  Q_UNUSED(event);
  if (isExposed())
  {
   if (!m_initialized)
	initialize();
   render();
   present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::exposeEvent(QExposeEvent* event)
 {
  Q_UNUSED(event);
  if (isExposed())
  {
   if (!m_initialized)
	initialize();
   render();
   present();
  }
 }

 void ArtifactDiligentEngineRenderWindow::keyPressEvent(QKeyEvent* event)
 {
  
 }

 void ArtifactDiligentEngineRenderWindow::mousePressEvent(QMouseEvent* event)
 {
  
 }

 class DiligentViewportWidget::Impl {
 private:

 public:
  Impl();
  ~Impl();
 };

 DiligentViewportWidget::Impl::Impl()
 {

 }

 DiligentViewportWidget::Impl::~Impl()
 {

 }

 void DiligentViewportWidget::keyPressEvent(QKeyEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 void DiligentViewportWidget::resizeEvent(QResizeEvent* event)
 {
  //throw std::logic_error("The method or operation is not implemented.");
 }

 DiligentViewportWidget::DiligentViewportWidget(QWidget* parent /*= nullptr*/):QWidget(parent)
 {

 }

 DiligentViewportWidget::~DiligentViewportWidget()
 {

 }

 void DiligentViewportWidget::initializeDiligentEngineSafely()
 {

 }

 QSize DiligentViewportWidget::sizeHint() const
 {
  
  return QSize(600,400);
 }

};
