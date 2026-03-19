module;
// ArtifactIRenderer maintenance rule:
// Do not rewrite the existing D3D12-specific path by guesswork.
// Do not replace this renderer with a Qt-only implementation.
// Extend backends carefully while preserving the current Diligent/D3D12 architecture.
#include <array>
#include <cstring>
#include <cstdint>
#include <QImage>
#include <QDebug>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Query.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>
#include <windows.h>

module Artifact.Render.IRenderer;

import Graphics;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Layer.Blend;
import Core.Scale.Zoom;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.ShaderManager;
import Artifact.Render.PrimitiveRenderer2D;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;
 using float2 = Diligent::float2;

 namespace {
  Diligent::float2 toDiligentFloat2(Detail::float2 value) { return { value.x, value.y }; }
  Detail::float2 toDetailFloat2(Diligent::float2 value)   { return { value.x, value.y }; }
 }

 // ---------------------------------------------------------------------------
 // Impl
 // ---------------------------------------------------------------------------
 class ArtifactIRenderer::Impl
 {
 private:
  DiligentDeviceManager deviceManager_;
  ShaderManager shaderManager_;
  PrimitiveRenderer2D primitiveRenderer_;

  RefCntAutoPtr<ITexture> m_layerRT;
  QWidget* widget_ = nullptr;

  bool m_initialized = false;
  bool m_frameQueryInitialized = false;
  double m_lastGpuFrameTimeMs = 0.0;
  Uint32 m_frameQueryIndex = 0;
  static constexpr Uint32 FrameQueryCount = 2;
  std::array<RefCntAutoPtr<IQuery>, FrameQueryCount> m_frameQueries;
  int m_offlineWidth  = 0;
  int m_offlineHeight = 0;

  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  const TEXTURE_FORMAT MAIN_RTV_FORMAT = TEX_FORMAT_RGBA8_UNORM_SRGB;

  void initFrameQueries();
  void createLayerRT(QWidget* window);

 public:
  explicit Impl(RefCntAutoPtr<IRenderDevice> device,
                RefCntAutoPtr<IDeviceContext> context, QWidget* widget);
  Impl();
  void present();
  ~Impl();

  void initialize(QWidget* parent);
  void initializeHeadless(int width, int height);
  QImage readbackToImage() const;
  void createSwapChain(QWidget* widget);
  void recreateSwapChain(QWidget* widget);
  void beginFrameGpuProfiling();
  void endFrameGpuProfiling();
  double lastFrameGpuTimeMs() const;

  void clear();
  void setClearColor(const FloatColor& color);
  void flushAndWait();
  void flush();
  void destroy();

  // Viewport
  void setViewportSize(float w, float h) { primitiveRenderer_.setViewportSize(w, h); }
  void setCanvasSize(float w, float h)   { primitiveRenderer_.setCanvasSize(w, h); }
  void setPan(float x, float y)          { primitiveRenderer_.setPan(x, y); }
  void setZoom(float zoom)               { primitiveRenderer_.setZoom(zoom); }
  float getZoom() const                  { return primitiveRenderer_.getZoom(); }
  void panBy(float dx, float dy)         { primitiveRenderer_.panBy(dx, dy); }
  void resetView()                       { primitiveRenderer_.resetView(); }
  void fitToViewport(float margin)       { primitiveRenderer_.fitToViewport(margin); }
  void zoomAroundViewportPoint(Detail::float2 pos, float newZoom)
  {
   primitiveRenderer_.zoomAroundViewportPoint(toDiligentFloat2(pos), newZoom);
  }
  Detail::float2 canvasToViewport(Detail::float2 pos) const
  {
   return toDetailFloat2(primitiveRenderer_.canvasToViewport(toDiligentFloat2(pos)));
  }
  Detail::float2 viewportToCanvas(Detail::float2 pos) const
  {
   return toDetailFloat2(primitiveRenderer_.viewportToCanvas(toDiligentFloat2(pos)));
  }

  // Draw (all delegate to primitiveRenderer_)
  void drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity)
  { primitiveRenderer_.drawRectLocal(x, y, w, h, color, opacity); }
  void drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
  { primitiveRenderer_.drawRectOutlineLocal(x, y, w, h, color); }
  void drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
  { primitiveRenderer_.drawRectOutlineLocal(x, y, w, h, color); }
  void drawRectOutline(float2 pos, float2 size, const FloatColor& color)
  { primitiveRenderer_.drawRectOutlineLocal(pos.x, pos.y, size.x, size.y, color); }
  void drawSolidLine(float2 start, float2 end, const FloatColor& color, float thickness)
  { primitiveRenderer_.drawThickLineLocal(start, end, thickness, color); }
  void drawSolidRect(float2 pos, float2 size, const FloatColor& color, float opacity)
  { primitiveRenderer_.drawRectLocal(pos.x, pos.y, size.x, size.y, color, opacity); }
  void drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity)
  { primitiveRenderer_.drawRectLocal(x, y, w, h, color, opacity); }
  void drawPoint(float x, float y, float size, const FloatColor& color)
  { primitiveRenderer_.drawPoint(x, y, size, color); }
  void drawSprite(float x, float y, float w, float h)
  { primitiveRenderer_.drawSpriteLocal(x, y, w, h, QImage()); }
  void drawSprite(float2 pos, float2 size)
  { primitiveRenderer_.drawSpriteLocal(pos.x, pos.y, size.x, size.y, QImage()); }
  void drawSpriteLocal(float x, float y, float w, float h, const QImage& image, float opacity)
  { primitiveRenderer_.drawSpriteLocal(x, y, w, h, image, opacity); }
  void drawThickLineLocal(float2 p1, float2 p2, float thickness, const FloatColor& color)
  { primitiveRenderer_.drawThickLineLocal(p1, p2, thickness, color); }
  void drawDotLineLocal(float2 p1, float2 p2, float thickness, float spacing, const FloatColor& color)
  { primitiveRenderer_.drawDotLineLocal(p1, p2, thickness, spacing, color); }
  void drawBezierLocal(float2 p0, float2 p1, float2 p2, float thickness, const FloatColor& color)
  { primitiveRenderer_.drawBezierLocal(p0, p1, p2, thickness, color); }
  void drawBezierLocal(float2 p0, float2 p1, float2 p2, float2 p3, float thickness, const FloatColor& color)
  { primitiveRenderer_.drawBezierLocal(p0, p1, p2, p3, thickness, color); }
  void drawSolidTriangleLocal(float2 p0, float2 p1, float2 p2, const FloatColor& color)
  { primitiveRenderer_.drawSolidTriangleLocal(p0, p1, p2, color); }
  void drawCheckerboard(float x, float y, float w, float h,
                        float tileSize, const FloatColor& c1, const FloatColor& c2)
  { primitiveRenderer_.drawCheckerboard(x, y, w, h, tileSize, c1, c2); }
  void drawGrid(float x, float y, float w, float h,
                float spacing, float thickness, const FloatColor& color)
  { primitiveRenderer_.drawGrid(x, y, w, h, spacing, thickness, color); }
  void drawParticles() {}
 };

 // ---------------------------------------------------------------------------
 // Impl constructors / destructor
 // ---------------------------------------------------------------------------

 ArtifactIRenderer::Impl::Impl(RefCntAutoPtr<IRenderDevice> device,
                                RefCntAutoPtr<IDeviceContext> context,
                                QWidget* widget)
  : deviceManager_(device, context), widget_(widget)
 {
 }

 ArtifactIRenderer::Impl::Impl()
  : deviceManager_()
 {
 }

 ArtifactIRenderer::Impl::~Impl() {}

 // ---------------------------------------------------------------------------
 // initialize
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::initialize(QWidget* widget)
 {
  widget_ = widget;
  deviceManager_.initialize(widget);

  if (!deviceManager_.isInitialized()) return;

  shaderManager_.initialize(deviceManager_.device(), MAIN_RTV_FORMAT);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();

  primitiveRenderer_.createBuffers(deviceManager_.device(), MAIN_RTV_FORMAT);
  primitiveRenderer_.setPSOs(shaderManager_);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                deviceManager_.swapChain());
  m_initialized = true;
 }

 void ArtifactIRenderer::Impl::initializeHeadless(int width, int height)
 {
  m_offlineWidth  = width;
  m_offlineHeight = height;

  deviceManager_.initializeHeadless();
  if (!deviceManager_.isInitialized()) return;

  shaderManager_.initialize(deviceManager_.device(), MAIN_RTV_FORMAT);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();

  primitiveRenderer_.createBuffers(deviceManager_.device(), MAIN_RTV_FORMAT);
  primitiveRenderer_.setPSOs(shaderManager_);

  TextureDesc TexDesc;
  TexDesc.Name      = "OfflineRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = static_cast<Uint32>(width);
  TexDesc.Height    = static_cast<Uint32>(height);
  TexDesc.MipLevels = 1;
  TexDesc.Format    = MAIN_RTV_FORMAT;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);

  auto* rtv = m_layerRT ? m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : nullptr;
  primitiveRenderer_.setOverrideRTV(rtv);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(), nullptr);
  primitiveRenderer_.setViewportSize(float(width), float(height));
  primitiveRenderer_.setCanvasSize(float(width), float(height));
  primitiveRenderer_.resetView();

  if (auto ctx = deviceManager_.immediateContext()) {
   Viewport VP;
   VP.Width = static_cast<float>(width);  VP.Height = static_cast<float>(height);
   VP.MinDepth = 0.0f; VP.MaxDepth = 1.0f; VP.TopLeftX = 0.0f; VP.TopLeftY = 0.0f;
   ctx->SetViewports(1, &VP, width, height);
  }
  m_initialized = true;
 }

 QImage ArtifactIRenderer::Impl::readbackToImage() const
 {
  auto ctx    = deviceManager_.immediateContext();
  auto device = deviceManager_.device();
  if (!ctx || !device) return {};

  // Resolve source texture and its dimensions.
  // Priority: headless offline RT > online layerRT > swap chain back buffer.
  ITexture* srcTex  = nullptr;
  Uint32 srcWidth   = 0;
  Uint32 srcHeight  = 0;

  if (m_layerRT && m_offlineWidth > 0 && m_offlineHeight > 0) {
   // Headless/offline mode: explicit dimensions stored on initializeHeadless().
   srcTex    = m_layerRT;
   srcWidth  = static_cast<Uint32>(m_offlineWidth);
   srcHeight = static_cast<Uint32>(m_offlineHeight);
  } else if (m_layerRT) {
   // Online mode: layerRT was sized from the widget via createLayerRT().
   const auto& desc = m_layerRT->GetDesc();
   srcTex    = m_layerRT;
   srcWidth  = desc.Width;
   srcHeight = desc.Height;
  } else if (auto sc = deviceManager_.swapChain()) {
   // Fallback: read directly from the swap chain back buffer.
   if (auto* rtv = sc->GetCurrentBackBufferRTV()) {
    srcTex = rtv->GetTexture();
    if (srcTex) {
     const auto& desc = srcTex->GetDesc();
     srcWidth  = desc.Width;
     srcHeight = desc.Height;
    }
   }
  }

  if (!srcTex || srcWidth == 0 || srcHeight == 0) return {};

  // Staging texture uses RGBA8_UNORM so the CPU can read raw bytes.
  // Diligent allows copying from a SRGB source to a linear staging texture
  // because both share the same memory layout (only the view interpretation differs).
  RefCntAutoPtr<ITexture> stagingTex;
  TextureDesc stagDesc;
  stagDesc.Name           = "ReadbackStagingTexture";
  stagDesc.Type           = RESOURCE_DIM_TEX_2D;
  stagDesc.Width          = srcWidth;
  stagDesc.Height         = srcHeight;
  stagDesc.MipLevels      = 1;
  stagDesc.Format         = TEX_FORMAT_RGBA8_UNORM;
  stagDesc.Usage          = USAGE_STAGING;
  stagDesc.CPUAccessFlags = CPU_ACCESS_READ;
  stagDesc.BindFlags      = BIND_NONE;
  device->CreateTexture(stagDesc, nullptr, &stagingTex);
  if (!stagingTex) return {};

  // Transition both textures to the required states and issue the copy.
  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture              = srcTex;
  copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copyAttribs.pDstTexture              = stagingTex;
  copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  ctx->CopyTexture(copyAttribs);

  // Use a fence to block the CPU until the GPU copy completes.
  RefCntAutoPtr<IFence> fence;
  FenceDesc fDesc;
  fDesc.Name = "ReadbackFence";
  fDesc.Type = FENCE_TYPE_GENERAL;
  device->CreateFence(fDesc, &fence);
  ctx->EnqueueSignal(fence, 1);
  ctx->Flush();
  fence->Wait(1);

  // Map the staging texture. MAP_FLAG_NONE (0) is safe here because the
  // fence->Wait(1) above guarantees the GPU copy has finished.
  MappedTextureSubresource mapped = {};
  ctx->MapTextureSubresource(stagingTex, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
  if (!mapped.pData) return {};

  QImage result(static_cast<int>(srcWidth), static_cast<int>(srcHeight), QImage::Format_RGBA8888);
  const auto* srcRow = static_cast<const uint8_t*>(mapped.pData);
  for (Uint32 row = 0; row < srcHeight; ++row) {
   std::memcpy(result.scanLine(static_cast<int>(row)), srcRow, static_cast<size_t>(srcWidth) * 4u);
   srcRow += mapped.Stride;
  }
  ctx->UnmapTextureSubresource(stagingTex, 0, 0);
  return result;
 }

 // ---------------------------------------------------------------------------
 // createSwapChain / recreateSwapChain
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::createLayerRT(QWidget* window)
 {
  if (!window || !deviceManager_.device()) return;
  if (m_layerRT) m_layerRT.Release();

  TextureDesc TexDesc;
  TexDesc.Name      = "LayerRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = static_cast<Uint32>(window->width()  * window->devicePixelRatio());
  TexDesc.Height    = static_cast<Uint32>(window->height() * window->devicePixelRatio());
  TexDesc.MipLevels = 1;
  TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);
 }

 void ArtifactIRenderer::Impl::createSwapChain(QWidget* window)
 {
  if (!window || !deviceManager_.device()) return;

  widget_ = window;
  deviceManager_.createSwapChain(window);

  if (!m_initialized) {
   shaderManager_.initialize(deviceManager_.device(), MAIN_RTV_FORMAT);
   shaderManager_.createShaders();
   shaderManager_.createPSOs();
   primitiveRenderer_.createBuffers(deviceManager_.device(), MAIN_RTV_FORMAT);
   primitiveRenderer_.setPSOs(shaderManager_);
   m_initialized = true;
  }

  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                deviceManager_.swapChain());
  createLayerRT(window);
 }

 void ArtifactIRenderer::Impl::recreateSwapChain(QWidget* widget)
 {
  if (!widget || !deviceManager_.device() || !deviceManager_.swapChain()) return;

  const int newWidth  = static_cast<int>(widget->width()  * widget->devicePixelRatio());
  const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
  if (newWidth <= 0 || newHeight <= 0) return;

  deviceManager_.recreateSwapChain(widget);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                deviceManager_.swapChain());
  createLayerRT(widget);
 }

 // ---------------------------------------------------------------------------
 // GPU frame profiling
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::initFrameQueries()
 {
  if (m_frameQueryInitialized || !deviceManager_.device()) return;

  QueryDesc desc;
  desc.Name = "FrameDurationQuery";
  desc.Type = QUERY_TYPE_DURATION;
  for (auto& query : m_frameQueries)
   deviceManager_.device()->CreateQuery(desc, &query);
  m_frameQueryInitialized = true;
 }

 void ArtifactIRenderer::Impl::beginFrameGpuProfiling()
 {
  initFrameQueries();
  auto& query = m_frameQueries[m_frameQueryIndex];
  if (!query || !deviceManager_.immediateContext()) return;
  deviceManager_.immediateContext()->BeginQuery(query);
 }

 void ArtifactIRenderer::Impl::endFrameGpuProfiling()
 {
  auto& query = m_frameQueries[m_frameQueryIndex];
  if (!query || !deviceManager_.immediateContext()) return;
  deviceManager_.immediateContext()->EndQuery(query);

  const Uint32 readIndex = (m_frameQueryIndex + FrameQueryCount - 1) % FrameQueryCount;
  auto& readQuery = m_frameQueries[readIndex];
  if (readQuery) {
   QueryDataDuration data;
   if (readQuery->GetData(&data, sizeof(data), True) && data.Frequency != 0)
    m_lastGpuFrameTimeMs = static_cast<double>(data.Duration) * 1000.0
                           / static_cast<double>(data.Frequency);
  }
  m_frameQueryIndex = (m_frameQueryIndex + 1) % FrameQueryCount;
 }

 double ArtifactIRenderer::Impl::lastFrameGpuTimeMs() const
 {
  return m_lastGpuFrameTimeMs;
 }

 // ---------------------------------------------------------------------------
 // clear / flush / destroy
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::clear()
 {
  primitiveRenderer_.clear(clearColor_);
 }

 void ArtifactIRenderer::Impl::setClearColor(const FloatColor& color)
 {
  clearColor_ = color;
 }

 void ArtifactIRenderer::Impl::flush()
 {
  if (auto ctx = deviceManager_.immediateContext())
   ctx->Flush();
 }

 void ArtifactIRenderer::Impl::flushAndWait()
 {
  auto device = deviceManager_.device();
  auto ctx    = deviceManager_.immediateContext();
  if (!device || !ctx) return;

  RefCntAutoPtr<IFence> fence;
  FenceDesc desc;
  desc.Name = "StopRenderLoopFence";
  desc.Type = FENCE_TYPE_GENERAL;
  device->CreateFence(desc, &fence);
  ctx->Flush();
  ctx->EnqueueSignal(fence, 1);
  ctx->Flush();
  fence->Wait(1);
 }

 void ArtifactIRenderer::Impl::destroy()
 {
  m_layerRT = nullptr;
  for (auto& query : m_frameQueries) query = nullptr;
  primitiveRenderer_.destroy();
  shaderManager_.destroy();
  deviceManager_.destroy();
  widget_                = nullptr;
  m_initialized          = false;
  m_frameQueryInitialized = false;
 }

 void ArtifactIRenderer::Impl::present()
 {
  if (auto sc = deviceManager_.swapChain())
   sc->Present();
 }

 // ---------------------------------------------------------------------------
 // ArtifactIRenderer public methods
 // ---------------------------------------------------------------------------

 ArtifactIRenderer::ArtifactIRenderer(RefCntAutoPtr<IRenderDevice> pDevice,
                                      RefCntAutoPtr<IDeviceContext> pImmediateContext,
                                      QWidget* widget)
  : impl_(std::make_unique<Impl>(pDevice, pImmediateContext, widget))
 {
 }

 ArtifactIRenderer::ArtifactIRenderer()
  : impl_(std::make_unique<Impl>())
 {
 }

 ArtifactIRenderer::~ArtifactIRenderer() = default;

 void ArtifactIRenderer::initialize(QWidget* widget)       { impl_->initialize(widget); }
 void ArtifactIRenderer::initializeHeadless(int w, int h)  { impl_->initializeHeadless(w, h); }
 void ArtifactIRenderer::createSwapChain(QWidget* widget)  { impl_->createSwapChain(widget); }
 void ArtifactIRenderer::recreateSwapChain(QWidget* widget){ impl_->recreateSwapChain(widget); }

 void ArtifactIRenderer::clear()        { impl_->clear(); }
 void ArtifactIRenderer::flush()        { impl_->flush(); }
 void ArtifactIRenderer::flushAndWait() { impl_->flushAndWait(); }
 void ArtifactIRenderer::destroy()      { impl_->destroy(); }

 QImage ArtifactIRenderer::readbackToImage() const { return impl_->readbackToImage(); }

 void ArtifactIRenderer::present()
 {
  impl_->present();
 }

 void ArtifactIRenderer::setClearColor(const FloatColor& color) { impl_->setClearColor(color); }
 void ArtifactIRenderer::setViewportSize(float w, float h) { impl_->setViewportSize(w, h); }
 void ArtifactIRenderer::setCanvasSize(float w, float h)        { impl_->setCanvasSize(w, h); }
 void ArtifactIRenderer::setPan(float x, float y)               { impl_->setPan(x, y); }
 void ArtifactIRenderer::setZoom(float zoom)                    { impl_->setZoom(zoom); }
 float ArtifactIRenderer::getZoom() const                       { return impl_->getZoom(); }
 void ArtifactIRenderer::panBy(float dx, float dy)              { impl_->panBy(dx, dy); }
 void ArtifactIRenderer::resetView()                            { impl_->resetView(); }
 void ArtifactIRenderer::fitToViewport(float margin)            { impl_->fitToViewport(margin); }

 void ArtifactIRenderer::zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom)
 { impl_->zoomAroundViewportPoint(viewportPos, newZoom); }
 Detail::float2 ArtifactIRenderer::canvasToViewport(Detail::float2 pos) const
 { return impl_->canvasToViewport(pos); }
 Detail::float2 ArtifactIRenderer::viewportToCanvas(Detail::float2 pos) const
 { return impl_->viewportToCanvas(pos); }

 void ArtifactIRenderer::drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
 { impl_->drawRectOutline(x, y, w, h, color); }
 void ArtifactIRenderer::drawRectOutline(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
 { impl_->drawRectOutline(toDiligentFloat2(pos), toDiligentFloat2(size), color); }
 void ArtifactIRenderer::drawSolidLine(Detail::float2 start, Detail::float2 end,
                                       const FloatColor& color, float thickness)
 { impl_->drawSolidLine(toDiligentFloat2(start), toDiligentFloat2(end), color, thickness); }
 void ArtifactIRenderer::drawSolidRect(float x, float y, float w, float h)
 { impl_->drawSolidRect(float2(x, y), float2(w, h), {1.0f, 1.0f, 1.0f, 1.0f}, 1.0f); }
 void ArtifactIRenderer::drawSolidRect(float x, float y, float w, float h, const FloatColor& color, float opacity)
 { impl_->drawSolidRect(float2(x, y), float2(w, h), color, opacity); }
 void ArtifactIRenderer::drawSolidRect(Detail::float2 pos, Detail::float2 size, const FloatColor& color, float opacity)
 { impl_->drawSolidRect(toDiligentFloat2(pos), toDiligentFloat2(size), color, opacity); }
 void ArtifactIRenderer::drawPoint(float x, float y, float size, const FloatColor& color)
 { impl_->drawPoint(x, y, size, color); }

 void ArtifactIRenderer::drawSprite(float x, float y, float w, float h)
 { impl_->drawSprite(x, y, w, h); }
 void ArtifactIRenderer::drawSprite(Detail::float2 pos, Detail::float2 size)
 { impl_->drawSprite(toDiligentFloat2(pos), toDiligentFloat2(size)); }
 void ArtifactIRenderer::drawSprite(float x, float y, float w, float h, const QImage& image, float opacity)
 { impl_->drawSpriteLocal(x, y, w, h, image, opacity); }
 void ArtifactIRenderer::drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity)
 { impl_->drawRectLocal(x, y, w, h, color, opacity); }
 void ArtifactIRenderer::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
 { impl_->drawRectOutlineLocal(x, y, w, h, color); }
 void ArtifactIRenderer::drawThickLineLocal(Detail::float2 p1, Detail::float2 p2,
                                            float thickness, const FloatColor& color)
 { impl_->drawThickLineLocal(toDiligentFloat2(p1), toDiligentFloat2(p2), thickness, color); }
 void ArtifactIRenderer::drawDotLineLocal(Detail::float2 p1, Detail::float2 p2,
                                          float thickness, float spacing, const FloatColor& color)
 { impl_->drawDotLineLocal(toDiligentFloat2(p1), toDiligentFloat2(p2), thickness, spacing, color); }
 void ArtifactIRenderer::drawBezierLocal(Detail::float2 p0, Detail::float2 p1,
                                         Detail::float2 p2, float thickness, const FloatColor& color)
 { impl_->drawBezierLocal(toDiligentFloat2(p0), toDiligentFloat2(p1),
                          toDiligentFloat2(p2), thickness, color); }
 void ArtifactIRenderer::drawBezierLocal(Detail::float2 p0, Detail::float2 p1,
                                         Detail::float2 p2, Detail::float2 p3,
                                         float thickness, const FloatColor& color)
 { impl_->drawBezierLocal(toDiligentFloat2(p0), toDiligentFloat2(p1),
                          toDiligentFloat2(p2), toDiligentFloat2(p3), thickness, color); }
 void ArtifactIRenderer::drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1,
                                                Detail::float2 p2, const FloatColor& color)
 { impl_->drawSolidTriangleLocal(toDiligentFloat2(p0), toDiligentFloat2(p1),
                                 toDiligentFloat2(p2), color); }
 void ArtifactIRenderer::drawCheckerboard(float x, float y, float w, float h,
                                          float tileSize, const FloatColor& c1, const FloatColor& c2)
 { impl_->drawCheckerboard(x, y, w, h, tileSize, c1, c2); }
 void ArtifactIRenderer::drawGrid(float x, float y, float w, float h,
                                  float spacing, float thickness, const FloatColor& color)
 { impl_->drawGrid(x, y, w, h, spacing, thickness, color); }
 void ArtifactIRenderer::drawParticles()                  { impl_->drawParticles(); }
 void ArtifactIRenderer::setUpscaleConfig(bool, float)    {}

} // namespace Artifact
