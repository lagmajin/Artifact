module;

#include <EngineFactory.h>
#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <RefCntAutoPtr.hpp>
#include <windows.h>

#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QExposeEvent>
#include <QResizeEvent>
#include <QString>
#include <QtMath>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>

module Artifact.Widgets.Timeline.DiligentRenderWindow;

import Graphics;
import Artifact.Render.Config;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.PrimitiveRenderer2D;
import Artifact.Render.RenderCommandBuffer;
import Artifact.Render.ShaderManager;
import Artifact.Render.DiligentImmediateSubmitter;
import Color.Float;

namespace {

QEvent::Type timelineGpuRenderEventType()
{
  static const int type = QEvent::registerEventType();
  return static_cast<QEvent::Type>(type);
}

Diligent::IEngineFactoryD3D12* resolveTimelineD3D12Factory()
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

Diligent::IEngineFactoryVk* resolveTimelineVkFactory()
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

} // namespace

namespace Artifact {

using namespace ArtifactCore;
using namespace Diligent;

class ArtifactDiligentTimelineRenderWindow::Impl {
public:
  mutable std::mutex snapshotMutex_;
  std::shared_ptr<const DiligentTimelineVisualSnapshot> snapshot_ =
      std::make_shared<const DiligentTimelineVisualSnapshot>();
  Diligent::RefCntAutoPtr<IRenderDevice> device_;
  Diligent::RefCntAutoPtr<IDeviceContext> immediateContext_;
  Diligent::RefCntAutoPtr<ISwapChain> swapChain_;
  ShaderManager shaderManager_;
  PrimitiveRenderer2D primitiveRenderer_;
  RenderCommandBuffer commandBuffer_;
  DiligentImmediateSubmitter submitter_;
  bool initialized_ = false;
  bool gpuReady_ = false;
  bool usingSharedDevice_ = false;
  std::atomic_bool renderEventPending_{false};

  static FloatColor toFloatColor(const QColor& color)
  {
    return FloatColor{
        static_cast<float>(color.redF()),
        static_cast<float>(color.greenF()),
        static_cast<float>(color.blueF()),
        static_cast<float>(color.alphaF())};
  }

  void releaseGpuResources()
  {
    if (immediateContext_) {
      immediateContext_->Flush();
      immediateContext_->WaitForIdle();
    }
    submitter_.destroy();
    primitiveRenderer_.destroy();
    shaderManager_.destroy();
    swapChain_.Release();
    immediateContext_.Release();
    device_.Release();
    if (usingSharedDevice_) {
      releaseSharedRenderDevice();
      usingSharedDevice_ = false;
    }
    gpuReady_ = false;
  }

  bool initialize(QWindow* window)
  {
    if (initialized_) {
      return gpuReady_;
    }
    initialized_ = true;

    const QString backend =
        qEnvironmentVariable("ARTIFACT_RENDER_BACKEND").toLower();
    if (backend == QStringLiteral("software") ||
        backend == QStringLiteral("sw")) {
      return false;
    }
    if (!acquireSharedRenderDeviceForCurrentBackend(device_, immediateContext_)) {
      initialized_ = false;
      return false;
    }
    usingSharedDevice_ = true;

    Win32NativeWindow nativeWindow;
    nativeWindow.hWnd = reinterpret_cast<HWND>(window->winId());
    SwapChainDesc swapChainDesc;
    swapChainDesc.ColorBufferFormat =
        RenderConfig::hdrDisplayEnabled()
            ? TEX_FORMAT_RGBA16_FLOAT
            : TEX_FORMAT_RGBA8_UNORM_SRGB;
    swapChainDesc.Width = static_cast<Uint32>(
        std::max(1, qRound(window->width() * window->devicePixelRatio())));
    swapChainDesc.Height = static_cast<Uint32>(
        std::max(1, qRound(window->height() * window->devicePixelRatio())));

    if (sharedRenderDeviceType() == RENDER_DEVICE_TYPE_VULKAN) {
      if (auto* factory = resolveTimelineVkFactory()) {
        factory->CreateSwapChainVk(device_, immediateContext_, swapChainDesc,
                                   nativeWindow, &swapChain_);
      }
    } else if (auto* factory = resolveTimelineD3D12Factory()) {
      FullScreenModeDesc fullScreenDesc;
      fullScreenDesc.Fullscreen = false;
      factory->CreateSwapChainD3D12(device_, immediateContext_, swapChainDesc,
                                     fullScreenDesc, nativeWindow, &swapChain_);
    }
    if (!swapChain_) {
      releaseGpuResources();
      initialized_ = false;
      return false;
    }

    shaderManager_.initialize(device_, swapChainDesc.ColorBufferFormat);
    shaderManager_.createShaders();
    shaderManager_.createPSOs();
    primitiveRenderer_.createBuffers(device_, swapChainDesc.ColorBufferFormat);
    primitiveRenderer_.setPSOs(shaderManager_);
    primitiveRenderer_.setContext(immediateContext_, swapChain_);
    primitiveRenderer_.setCommandBuffer(&commandBuffer_);
    submitter_.createBuffers(device_, swapChainDesc.ColorBufferFormat);
    submitter_.setPSOs(shaderManager_);
    gpuReady_ = true;
    return true;
  }

  void render(QWindow* window)
  {
    if (!gpuReady_ || !swapChain_ || !immediateContext_ ||
        !window->isExposed()) {
      return;
    }

    std::shared_ptr<const DiligentTimelineVisualSnapshot> snapshot;
    {
      std::scoped_lock lock(snapshotMutex_);
      snapshot = snapshot_;
    }

    Diligent::ITextureView* rtv = swapChain_->GetCurrentBackBufferRTV();
    immediateContext_->SetRenderTargets(
        1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const FloatColor background = toFloatColor(snapshot->background);
    const float clearColor[] = {
        background.r(), background.g(), background.b(), background.a()};
    immediateContext_->ClearRenderTarget(
        rtv, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    commandBuffer_.reset();
    commandBuffer_.targetRTV = rtv;
    primitiveRenderer_.setOverrideRTV(rtv);
    primitiveRenderer_.setViewportSize(
        static_cast<float>(window->width()),
        static_cast<float>(window->height()));
    primitiveRenderer_.setCanvasSize(
        static_cast<float>(window->width()),
        static_cast<float>(window->height()));
    primitiveRenderer_.setDevicePixelRatio(
        static_cast<float>(window->devicePixelRatio()));
    primitiveRenderer_.resetView();

    for (const auto& visual : snapshot->rects) {
      primitiveRenderer_.drawSolidRect(
          static_cast<float>(visual.rect.x()),
          static_cast<float>(visual.rect.y()),
          static_cast<float>(visual.rect.width()),
          static_cast<float>(visual.rect.height()),
          toFloatColor(visual.color));
    }
    for (const auto& visual : snapshot->lines) {
      primitiveRenderer_.drawThickLineLocal(
          {static_cast<float>(visual.from.x()),
           static_cast<float>(visual.from.y())},
          {static_cast<float>(visual.to.x()),
           static_cast<float>(visual.to.y())},
          visual.thickness, toFloatColor(visual.color));
    }
    for (const auto& visual : snapshot->triangles) {
      primitiveRenderer_.drawSolidTriangleLocal(
          {static_cast<float>(visual.p0.x()),
           static_cast<float>(visual.p0.y())},
          {static_cast<float>(visual.p1.x()),
           static_cast<float>(visual.p1.y())},
          {static_cast<float>(visual.p2.x()),
           static_cast<float>(visual.p2.y())},
          toFloatColor(visual.color));
    }
    submitter_.submit(commandBuffer_, immediateContext_);
    swapChain_->Present();
  }
};

ArtifactDiligentTimelineRenderWindow::ArtifactDiligentTimelineRenderWindow(
    QWindow* parent)
    : QWindow(parent), impl_(new Impl())
{
  setSurfaceType(QSurface::RasterSurface);
}

ArtifactDiligentTimelineRenderWindow::~ArtifactDiligentTimelineRenderWindow()
{
  impl_->releaseGpuResources();
  delete impl_;
  impl_ = nullptr;
}

void ArtifactDiligentTimelineRenderWindow::setSnapshot(
    const DiligentTimelineVisualSnapshot& snapshot)
{
  {
    std::scoped_lock lock(impl_->snapshotMutex_);
    if (snapshot.generation < impl_->snapshot_->generation) {
      return;
    }
    impl_->snapshot_ =
        std::make_shared<const DiligentTimelineVisualSnapshot>(snapshot);
  }
  requestRender();
}

quint64 ArtifactDiligentTimelineRenderWindow::snapshotGeneration() const
{
  std::scoped_lock lock(impl_->snapshotMutex_);
  return impl_->snapshot_->generation;
}

bool ArtifactDiligentTimelineRenderWindow::initialize()
{
  return impl_->initialize(this);
}

bool ArtifactDiligentTimelineRenderWindow::isGpuReady() const
{
  return impl_->gpuReady_;
}

void ArtifactDiligentTimelineRenderWindow::requestRender()
{
  if (!impl_->renderEventPending_.exchange(true, std::memory_order_acq_rel)) {
    QCoreApplication::postEvent(this, new QEvent(timelineGpuRenderEventType()));
  }
}

bool ArtifactDiligentTimelineRenderWindow::event(QEvent* event)
{
  if (event && event->type() == timelineGpuRenderEventType()) {
    impl_->renderEventPending_.store(false, std::memory_order_release);
    if (isExposed() && (impl_->gpuReady_ || initialize())) {
      impl_->render(this);
    }
    return true;
  }
  return QWindow::event(event);
}

void ArtifactDiligentTimelineRenderWindow::resizeEvent(QResizeEvent* event)
{
  QWindow::resizeEvent(event);
  if (impl_->swapChain_) {
    impl_->swapChain_->Resize(
        static_cast<Uint32>(std::max(
            1, qRound(width() * devicePixelRatio()))),
        static_cast<Uint32>(std::max(
            1, qRound(height() * devicePixelRatio()))));
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::exposeEvent(QExposeEvent* event)
{
  QWindow::exposeEvent(event);
  requestRender();
}

} // namespace Artifact
