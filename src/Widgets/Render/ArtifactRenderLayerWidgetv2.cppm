module;
#define NOMINMAX
#include <windows.h>
#include <cstring>
#include <tbb/tbb.h>
#include <QList>
#include <d3d12.h>
//#include <d3>
#include <RenderDevice.h>
#include <DeviceContext.h>
//#include <DiligentCore/Common/interface/map>

#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DeviceContextD3D12.h>
#include <SwapChain.h>
#include <RefCntAutoPtr.hpp>
#include <DiligentTools/Imgui/interface/ImGuiDiligentRenderer.hpp>


#include <wobjectimpl.h>
#include <RenderDeviceD3D12.h>
#include <QTimer>
#include <QDebug>
#include <QKeyEvent>
#include <QHashFunctions>

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

 using namespace ArtifactCore;

namespace {
  enum class RenderBackendPreference {
   Auto,
   D3D12,
   Vulkan
  };

  RenderBackendPreference getBackendPreferenceFromEnv()
  {
   char value[64] = {};
   const DWORD len = ::GetEnvironmentVariableA("ARTIFACT_RENDER_BACKEND", value, static_cast<DWORD>(sizeof(value)));
   if (len == 0 || len >= sizeof(value)) {
    return RenderBackendPreference::Auto;
   }
   CharLowerA(value);
   if (strcmp(value, "vulkan") == 0 || strcmp(value, "vk") == 0) {
    return RenderBackendPreference::Vulkan;
   }
   if (strcmp(value, "d3d12") == 0 || strcmp(value, "dx12") == 0) {
    return RenderBackendPreference::D3D12;
   }
   return RenderBackendPreference::Auto;
  }

  Diligent::IEngineFactoryVk* resolveVkFactoryFromDll()
  {
   using GetFactoryFn = Diligent::IEngineFactoryVk* (*)();
   static const wchar_t* kDllCandidates[] = {
    L"GraphicsEngineVk_64d.dll",
    L"GraphicsEngineVk_64r.dll",
    L"GraphicsEngineVk.dll"
   };
   for (const auto* dllName : kDllCandidates) {
    HMODULE mod = ::GetModuleHandleW(dllName);
    if (!mod) {
     mod = ::LoadLibraryW(dllName);
    }
    if (!mod) {
     continue;
    }
    auto* fn = reinterpret_cast<GetFactoryFn>(::GetProcAddress(mod, "GetEngineFactoryVk"));
    if (!fn) {
     fn = reinterpret_cast<GetFactoryFn>(::GetProcAddress(mod, "Diligent_GetEngineFactoryVk"));
    }
    if (fn) {
     return fn();
    }
   }
   return nullptr;
  }

  Diligent::IEngineFactoryD3D12* resolveD3D12FactoryFromDll()
  {
   using GetFactoryFn = Diligent::IEngineFactoryD3D12* (*)();
   static const wchar_t* kDllCandidates[] = {
    L"GraphicsEngineD3D12_64d.dll",
    L"GraphicsEngineD3D12_64r.dll",
    L"GraphicsEngineD3D12.dll"
   };
   for (const auto* dllName : kDllCandidates) {
    HMODULE mod = ::GetModuleHandleW(dllName);
    if (!mod) {
     mod = ::LoadLibraryW(dllName);
    }
    if (!mod) {
     continue;
    }
    auto* fn = reinterpret_cast<GetFactoryFn>(::GetProcAddress(mod, "GetEngineFactoryD3D12"));
    if (!fn) {
     fn = reinterpret_cast<GetFactoryFn>(::GetProcAddress(mod, "Diligent_GetEngineFactoryD3D12"));
    }
    if (fn) {
     return fn();
    }
   }
   return nullptr;
  }
 }

W_OBJECT_IMPL(ArtifactLayerEditorWidgetV2)
 void ArtifactLayerEditorWidgetV2::play() {}
 void ArtifactLayerEditorWidgetV2::stop() {}
 void ArtifactLayerEditorWidgetV2::takeScreenShot() {}

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
  std::unique_ptr<ArtifactIRenderer> renderer_;
  bool initialized_ = false;
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
  LayerID targetLayerId_{};
  FloatColor targetLayerTint_{ 1.0f, 0.5f, 0.5f, 1.0f };
  
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
 {
  widget_ = window;
  const auto backendPref = getBackendPreferenceFromEnv();

  auto tryInitVk = [&]() -> bool
  {
   auto* pFactoryVk = resolveVkFactoryFromDll();
   if (!pFactoryVk) {
    return false;
   }
   EngineVkCreateInfo creationAttribs = {};
   creationAttribs.EnableValidation = true;
   creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
   pFactoryVk->CreateDeviceAndContextsVk(creationAttribs, &pDevice, &pImmediateContext);
   if (!pDevice || !pImmediateContext) {
    return false;
   }
   renderer_ = std::make_unique<ArtifactIRenderer>(pDevice, pImmediateContext, window);
   renderer_->createSwapChain(window);
   qDebug() << "[ArtifactLayerEditorWidgetV2] Initialized with direct Diligent Vulkan device/context.";
   return true;
  };

  auto tryInitD3D12 = [&]() -> bool
  {
   auto* pFactoryD3D12 = resolveD3D12FactoryFromDll();
   if (!pFactoryD3D12) {
    return false;
   }
   EngineD3D12CreateInfo creationAttribs = {};
   creationAttribs.EnableValidation = true;
   creationAttribs.SetValidationLevel(Diligent::VALIDATION_LEVEL_2);
   pFactoryD3D12->CreateDeviceAndContextsD3D12(creationAttribs, &pDevice, &pImmediateContext);
   if (!pDevice || !pImmediateContext) {
    return false;
   }
   renderer_ = std::make_unique<ArtifactIRenderer>(pDevice, pImmediateContext, window);
   renderer_->createSwapChain(window);
   qDebug() << "[ArtifactLayerEditorWidgetV2] Initialized with direct Diligent D3D12 device/context.";
   return true;
  };

  bool initializedDirect = false;
  switch (backendPref) {
  case RenderBackendPreference::Vulkan:
   initializedDirect = tryInitVk();
   if (!initializedDirect) {
    initializedDirect = tryInitD3D12();
   }
   break;
  case RenderBackendPreference::D3D12:
   initializedDirect = tryInitD3D12();
   if (!initializedDirect) {
    initializedDirect = tryInitVk();
   }
   break;
  case RenderBackendPreference::Auto:
  default:
   initializedDirect = tryInitD3D12();
   if (!initializedDirect) {
    initializedDirect = tryInitVk();
   }
   break;
  }

  if (!initializedDirect) {
   renderer_ = std::make_unique<ArtifactIRenderer>();
   renderer_->initialize(window);
   qWarning() << "[ArtifactLayerEditorWidgetV2] Falling back to ArtifactIRenderer internal initialization.";
  }

  initialized_ = true;
 }

 void ArtifactLayerEditorWidgetV2::Impl::initializeSwapChain(QWidget* window)
 {
  if (!renderer_) {
   return;
  }
  renderer_->recreateSwapChain(window);
 }

 void ArtifactLayerEditorWidgetV2::Impl::destroy()
 {
  stopRenderLoop();
  if (renderer_) {
   renderer_->destroy();
  }
  initialized_ = false;
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
	 {
	  std::lock_guard<std::mutex> lock(resizeMutex_);
	  renderOneFrame();
	 }
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

  if (renderer_) {
   renderer_->flushAndWait();
  }
 }

 void ArtifactLayerEditorWidgetV2::Impl::renderOneFrame()
 {
 if (!initialized_ || !renderer_)
  return;
 renderer_->clear();
  renderer_->drawRectLocal(0,0, 400, 450, targetLayerTint_);
  renderer_->flush();
  renderer_->present();
}

 void ArtifactLayerEditorWidgetV2::Impl::recreateSwapChain(QWidget* window)
 {
  if (!initialized_ || !renderer_) {
   return;
  }
  std::lock_guard<std::mutex> lock(resizeMutex_);
  renderer_->recreateSwapChain(window);
 }

 ArtifactLayerEditorWidgetV2::ArtifactLayerEditorWidgetV2(QWidget* parent /*= nullptr*/) :QWidget(parent), impl_(new Impl())
 {
  setMinimumSize(1, 1);

  setFocusPolicy(Qt::StrongFocus);
  setAttribute(Qt::WA_NativeWindow);
  setAttribute(Qt::WA_PaintOnScreen);
  setAttribute(Qt::WA_NoSystemBackground);

  setWindowTitle("ArtifactLayerEditor");

 }

 ArtifactLayerEditorWidgetV2::~ArtifactLayerEditorWidgetV2()
 {
  impl_->destroy();
  delete impl_;
  impl_ = nullptr;
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
  if (!impl_->initialized_) {
   impl_->initialize(this);
   impl_->initializeSwapChain(this);
   impl_->startRenderLoop();
  }
 }
 void ArtifactLayerEditorWidgetV2::closeEvent(QCloseEvent* event)
 {
  impl_->destroy();
  QWidget::closeEvent(event);
 }

 void ArtifactLayerEditorWidgetV2::focusInEvent(QFocusEvent* event)
 {

 }

 void ArtifactLayerEditorWidgetV2::setClearColor(const FloatColor& color)
 {

 }

void ArtifactLayerEditorWidgetV2::setTargetLayer(const LayerID& id)
{
 std::lock_guard<std::mutex> lock(impl_->resizeMutex_);
 impl_->targetLayerId_ = id;
 const uint seed = qHash(id.toString());
 const auto channel = [seed](int shift) -> float {
  const int value = static_cast<int>((seed >> shift) & 0xFFu);
  return 0.25f + (static_cast<float>(value) / 255.0f) * 0.65f;
 };
 impl_->targetLayerTint_ = FloatColor(channel(0), channel(8), channel(16), 1.0f);
 if (impl_->renderer_) {
  impl_->renderer_->resetView();
 }
}

 void ArtifactLayerEditorWidgetV2::resetView()
 {
  if (impl_->renderer_) impl_->renderer_->resetView();
 }
 
 void ArtifactLayerEditorWidgetV2::fitToViewport()
 {
  if (impl_->renderer_) impl_->renderer_->fitToViewport();
 }
 
 void ArtifactLayerEditorWidgetV2::panBy(const QPointF& delta)
 {
  if (impl_->renderer_) impl_->renderer_->panBy((float)delta.x(), (float)delta.y());
 }

 void ArtifactLayerEditorWidgetV2::zoomAroundPoint(const QPointF& viewportPos, float newZoom)
 {
  if (impl_->renderer_) {
      impl_->renderer_->zoomAroundViewportPoint({(float)viewportPos.x(), (float)viewportPos.y()}, newZoom);
  }
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
