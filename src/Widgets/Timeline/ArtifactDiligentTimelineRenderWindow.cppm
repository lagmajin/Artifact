module;

#include <EngineFactory.h>
#include <EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <RefCntAutoPtr.hpp>
#include <windows.h>

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QExposeEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QSize>
#include <QString>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QtMath>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>

module Artifact.Widgets.Timeline.DiligentRenderWindow;

import Graphics;
import Artifact.Render.Config;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.PrimitiveRenderer2D;
import Artifact.Render.RenderCommandBuffer;
import Artifact.Render.ShaderManager;
import Artifact.Render.DiligentImmediateSubmitter;
import Color.Float;
import Text.Style;
import Utils.String.UniString;

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
  std::shared_ptr<const DiligentTimelineVisualSnapshot> staticSnapshot_;
  std::shared_ptr<const DiligentTimelineVisualSnapshot> dynamicSnapshot_;
  bool layeredSnapshots_ = false;
  Diligent::RefCntAutoPtr<IRenderDevice> device_;
  Diligent::RefCntAutoPtr<IDeviceContext> immediateContext_;
  Diligent::RefCntAutoPtr<ISwapChain> swapChain_;
  ShaderManager shaderManager_;
  PrimitiveRenderer2D primitiveRenderer_;
  RenderCommandBuffer commandBuffer_;
  DiligentImmediateSubmitter submitter_;
  bool initialized_ = false;
  bool gpuReady_ = false;
  bool ownsIndependentDevice_ = false;
  std::atomic_bool renderEventPending_{false};
  // Keep the idle surface inexpensive, but give direct manipulation a full
  // 60 Hz presentation lane. Skipped frames retain only the latest snapshot.
  // GUI thread only.
  static constexpr std::chrono::milliseconds kIdlePresentInterval{33};
  static constexpr std::chrono::milliseconds kInteractivePresentInterval{16};
  std::chrono::steady_clock::time_point lastPresent_{};
  bool throttleWakeupPending_ = false;
  QPointer<QWidget> inputTarget_;
  std::function<bool(const QPointF&, const QPoint&, Qt::KeyboardModifiers)>
      wheelInputHandler_;
  std::function<bool(Qt::MouseButton, const QPointF&, Qt::MouseButtons,
                     Qt::KeyboardModifiers)> panInputHandler_;
  std::function<void()> inputUpdatedCallback_;
  std::function<bool()> interactionStateProvider_;
  std::chrono::steady_clock::time_point lastMouseMoveSnapshot_{};

  static FloatColor toFloatColor(const QColor& color)
  {
    const auto srgbToLinear = [](const float channel) {
      return channel <= 0.04045f
                 ? channel / 12.92f
                 : static_cast<float>(
                       std::pow((channel + 0.055f) / 1.055f, 2.4f));
    };
    return FloatColor{
        srgbToLinear(static_cast<float>(color.redF())),
        srgbToLinear(static_cast<float>(color.greenF())),
        srgbToLinear(static_cast<float>(color.blueF())),
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
    ownsIndependentDevice_ = false;
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
    // Timeline owns a fully independent device: the VP worker thread keeps
    // the shared immediate context to itself, so the two surfaces never
    // submit through the same context. Failure means no GPU (the caller
    // falls back to the QWidget painter); there is no shared fallback.
    IndependentRenderDevice independent;
    if (!createIndependentRenderDevice(QStringLiteral("timeline"), -1,
                                       independent)) {
      initialized_ = false;
      return false;
    }
    device_ = independent.device;
    immediateContext_ = independent.immediateContext;
    independent.device.Release();
    independent.immediateContext.Release();
    ownsIndependentDevice_ = true;

    Win32NativeWindow nativeWindow;
    nativeWindow.hWnd = reinterpret_cast<HWND>(window->winId());
    SwapChainDesc swapChainDesc;
    // ShaderManager and its shared PSOs require this exact back-buffer format.
    // QColor values are converted to linear above before reaching the sRGB RTV.
    swapChainDesc.ColorBufferFormat =
        RenderConfig::hdrDisplayEnabled()
            ? TEX_FORMAT_RGBA16_FLOAT
            : TEX_FORMAT_RGBA8_UNORM_SRGB;
    // 2D-only surface: no depth buffer (render uses a null DSV throughout).
    // IsPrimary stays true so this device's stale resources are released on
    // its own Present; see SwapChainDesc docs before touching it.
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
    swapChainDesc.Width = static_cast<Uint32>(
        std::max(1, qRound(window->width() * window->devicePixelRatio())));
    swapChainDesc.Height = static_cast<Uint32>(
        std::max(1, qRound(window->height() * window->devicePixelRatio())));

    if (device_->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_VULKAN) {
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

    const auto now = std::chrono::steady_clock::now();
    const bool interacting = interactionStateProvider_ && interactionStateProvider_();
    const auto minPresentInterval = interacting ? kInteractivePresentInterval
                                                : kIdlePresentInterval;
    if (now - lastPresent_ < minPresentInterval) {
      if (!throttleWakeupPending_) {
        throttleWakeupPending_ = true;
        const auto wait = minPresentInterval - (now - lastPresent_);
        QTimer::singleShot(
            static_cast<int>(wait.count()), window, [this, window]() {
              throttleWakeupPending_ = false;
              if (!renderEventPending_.exchange(
                      true, std::memory_order_acq_rel)) {
                QCoreApplication::postEvent(
                    window, new QEvent(timelineGpuRenderEventType()));
              }
            });
      }
      return;
    }
    lastPresent_ = now;

    std::shared_ptr<const DiligentTimelineVisualSnapshot> snapshot;
    std::shared_ptr<const DiligentTimelineVisualSnapshot> staticSnapshot;
    std::shared_ptr<const DiligentTimelineVisualSnapshot> dynamicSnapshot;
    bool layeredSnapshots = false;
    {
      std::scoped_lock lock(snapshotMutex_);
      snapshot = snapshot_;
      staticSnapshot = staticSnapshot_;
      dynamicSnapshot = dynamicSnapshot_;
      layeredSnapshots = layeredSnapshots_;
    }

    Diligent::ITextureView* rtv = swapChain_->GetCurrentBackBufferRTV();
    immediateContext_->SetRenderTargets(
        1, &rtv, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const QColor& backgroundColor = layeredSnapshots && staticSnapshot
        ? staticSnapshot->background
        : snapshot->background;
    const FloatColor background = toFloatColor(backgroundColor);
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

    struct ColorCacheEntry {
      QRgb key = 0;
      FloatColor value{};
      bool valid = false;
    };
    ColorCacheEntry colorCache[32]{};
    size_t nextColorSlot = 0;
    const auto cachedColor = [&](const QColor& color) -> const FloatColor& {
      const QRgb key = color.rgba();
      for (auto& entry : colorCache) {
        if (entry.valid && entry.key == key) {
          return entry.value;
        }
      }
      auto& entry = colorCache[nextColorSlot++ % 32];
      entry.key = key;
      entry.value = toFloatColor(color);
      entry.valid = true;
      return entry.value;
    };
    const auto drawSnapshot = [this, &cachedColor](const DiligentTimelineVisualSnapshot& source) {
    for (const auto& visual : source.rects) {
      primitiveRenderer_.drawSolidRect(
          static_cast<float>(visual.rect.x()),
          static_cast<float>(visual.rect.y()),
          static_cast<float>(visual.rect.width()),
          static_cast<float>(visual.rect.height()),
          cachedColor(visual.color));
    }
    for (const auto& visual : source.lines) {
      primitiveRenderer_.drawThickLineLocal(
          {static_cast<float>(visual.from.x()),
           static_cast<float>(visual.from.y())},
          {static_cast<float>(visual.to.x()),
           static_cast<float>(visual.to.y())},
          visual.thickness, cachedColor(visual.color));
    }
    for (const auto& visual : source.triangles) {
      primitiveRenderer_.drawSolidTriangleLocal(
          {static_cast<float>(visual.p0.x()),
           static_cast<float>(visual.p0.y())},
          {static_cast<float>(visual.p1.x()),
           static_cast<float>(visual.p1.y())},
          {static_cast<float>(visual.p2.x()),
           static_cast<float>(visual.p2.y())},
          cachedColor(visual.color));
    }
    for (const auto& visual : source.texts) {
      ArtifactCore::TextStyle textStyle;
      textStyle.fontSize = visual.pixelSize;
      textStyle.pixelSize = visual.pixelSize;
      primitiveRenderer_.drawGlyphText(
          static_cast<float>(visual.baseline.x()),
          static_cast<float>(visual.baseline.y()),
          visual.text, textStyle,
          cachedColor(visual.color));
    }
    };
    if (layeredSnapshots) {
      if (staticSnapshot) {
        drawSnapshot(*staticSnapshot);
      }
      if (dynamicSnapshot) {
        drawSnapshot(*dynamicSnapshot);
      }
    } else {
      drawSnapshot(*snapshot);
    }
    submitter_.submit(commandBuffer_, immediateContext_);
    // SyncInterval 0: the timeline surface must never stall the GUI thread
    // on vsync. Tearing on tracks/playhead is acceptable here; VP keeps vsync.
    swapChain_->Present(0);
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
    impl_->layeredSnapshots_ = false;
    impl_->staticSnapshot_.reset();
    impl_->dynamicSnapshot_.reset();
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::setSnapshot(
    DiligentTimelineVisualSnapshot&& snapshot)
{
  {
    std::scoped_lock lock(impl_->snapshotMutex_);
    if (snapshot.generation < impl_->snapshot_->generation) {
      return;
    }
    impl_->snapshot_ =
        std::make_shared<const DiligentTimelineVisualSnapshot>(
            std::move(snapshot));
    impl_->layeredSnapshots_ = false;
    impl_->staticSnapshot_.reset();
    impl_->dynamicSnapshot_.reset();
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::setStaticSnapshot(
    const DiligentTimelineVisualSnapshot& snapshot)
{
  {
    std::scoped_lock lock(impl_->snapshotMutex_);
    if (impl_->staticSnapshot_ &&
        snapshot.generation < impl_->staticSnapshot_->generation) {
      return;
    }
    impl_->staticSnapshot_ =
        std::make_shared<const DiligentTimelineVisualSnapshot>(snapshot);
    impl_->layeredSnapshots_ = true;
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::setStaticSnapshot(
    DiligentTimelineVisualSnapshot&& snapshot)
{
  {
    std::scoped_lock lock(impl_->snapshotMutex_);
    if (impl_->staticSnapshot_ &&
        snapshot.generation < impl_->staticSnapshot_->generation) {
      return;
    }
    impl_->staticSnapshot_ =
        std::make_shared<const DiligentTimelineVisualSnapshot>(
            std::move(snapshot));
    impl_->layeredSnapshots_ = true;
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::setDynamicSnapshot(
    DiligentTimelineVisualSnapshot&& snapshot)
{
  {
    std::scoped_lock lock(impl_->snapshotMutex_);
    if (impl_->dynamicSnapshot_ &&
        snapshot.generation < impl_->dynamicSnapshot_->generation) {
      return;
    }
    impl_->dynamicSnapshot_ =
        std::make_shared<const DiligentTimelineVisualSnapshot>(
            std::move(snapshot));
    impl_->layeredSnapshots_ = true;
  }
  requestRender();
}

quint64 ArtifactDiligentTimelineRenderWindow::snapshotGeneration() const
{
  std::scoped_lock lock(impl_->snapshotMutex_);
  if (impl_->layeredSnapshots_) {
    const quint64 staticGeneration = impl_->staticSnapshot_
        ? impl_->staticSnapshot_->generation
        : 0;
    const quint64 dynamicGeneration = impl_->dynamicSnapshot_
        ? impl_->dynamicSnapshot_->generation
        : 0;
    return std::max(staticGeneration, dynamicGeneration);
  }
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

void ArtifactDiligentTimelineRenderWindow::setInputTarget(QWidget* target)
{
  impl_->inputTarget_ = target;
  if (impl_->inputTarget_ && width() > 0 && height() > 0) {
    impl_->inputTarget_->resize(width(), height());
  }
}

void ArtifactDiligentTimelineRenderWindow::setWheelInputHandler(
    std::function<bool(const QPointF&, const QPoint&, Qt::KeyboardModifiers)> handler)
{
  impl_->wheelInputHandler_ = std::move(handler);
}

void ArtifactDiligentTimelineRenderWindow::setPanInputHandler(
    std::function<bool(Qt::MouseButton, const QPointF&, Qt::MouseButtons,
                       Qt::KeyboardModifiers)> handler)
{
  impl_->panInputHandler_ = std::move(handler);
}

void ArtifactDiligentTimelineRenderWindow::setInputUpdatedCallback(
    std::function<void()> callback)
{
  impl_->inputUpdatedCallback_ = std::move(callback);
}

void ArtifactDiligentTimelineRenderWindow::setInteractionStateProvider(
    std::function<bool()> provider)
{
  impl_->interactionStateProvider_ = std::move(provider);
}

void ArtifactDiligentTimelineRenderWindow::requestRender()
{
  if (!impl_->renderEventPending_.exchange(true, std::memory_order_acq_rel)) {
    QCoreApplication::postEvent(this, new QEvent(timelineGpuRenderEventType()));
  }
}

bool ArtifactDiligentTimelineRenderWindow::event(QEvent* event)
{
  if (event && impl_->panInputHandler_ &&
      (event->type() == QEvent::MouseButtonPress ||
       event->type() == QEvent::MouseMove ||
       event->type() == QEvent::MouseButtonRelease)) {
    const auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (impl_->panInputHandler_(mouseEvent->button(), mouseEvent->position(),
                                mouseEvent->buttons(), mouseEvent->modifiers())) {
      event->accept();
      if (impl_->inputUpdatedCallback_) impl_->inputUpdatedCallback_();
      return true;
    }
  }
  if (event && event->type() == QEvent::Wheel && impl_->wheelInputHandler_) {
    const auto* wheelEvent = static_cast<QWheelEvent*>(event);
    if (impl_->wheelInputHandler_(wheelEvent->position(),
                                  wheelEvent->angleDelta(),
                                  wheelEvent->modifiers())) {
      event->accept();
      if (impl_->inputUpdatedCallback_) impl_->inputUpdatedCallback_();
      return true;
    }
  }
  if (event && impl_->inputTarget_) {
    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    {
      if (impl_->inputTarget_->size() != QSize(width(), height())) {
        impl_->inputTarget_->resize(width(), height());
      }
      if (event->type() == QEvent::MouseButtonPress) {
        requestActivate();
      }
      QCoreApplication::sendEvent(impl_->inputTarget_, event);
      setCursor(impl_->inputTarget_->cursor());
      const auto now = std::chrono::steady_clock::now();
      const bool isMouseMove = event->type() == QEvent::MouseMove;
      const bool snapshotDue = !isMouseMove ||
          now - impl_->lastMouseMoveSnapshot_ >= std::chrono::milliseconds(16);
      if (impl_->inputUpdatedCallback_ && snapshotDue) {
        if (isMouseMove) {
          impl_->lastMouseMoveSnapshot_ = now;
        }
        impl_->inputUpdatedCallback_();
      }
      return true;
    }
    default:
      break;
    }
  }
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
  if (impl_->inputTarget_) {
    impl_->inputTarget_->resize(event->size());
  }
  if (impl_->swapChain_) {
    impl_->swapChain_->Resize(
        static_cast<Uint32>(std::max(
            1, qRound(width() * devicePixelRatio()))),
        static_cast<Uint32>(std::max(
            1, qRound(height() * devicePixelRatio()))));
  }
  // A window-container page can receive its final size after the Timeline has
  // already queued its first visual snapshot. Rebuild it after the Qt input
  // surface has the native window's settled geometry; otherwise a Curve Editor
  // snapshot can be skipped for its temporary zero-sized plot.
  if (impl_->inputUpdatedCallback_) {
    impl_->inputUpdatedCallback_();
  }
  requestRender();
}

void ArtifactDiligentTimelineRenderWindow::exposeEvent(QExposeEvent* event)
{
  QWindow::exposeEvent(event);
  // A stacked-page switch can expose an already-sized native window without a
  // subsequent resize. Use the same coalesced callback to populate its first
  // visible frame from the settled layout.
  if (isExposed() && impl_->inputUpdatedCallback_) {
    impl_->inputUpdatedCallback_();
  }
  requestRender();
}

} // namespace Artifact
