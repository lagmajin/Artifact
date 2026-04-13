module;
#include <utility>
// ArtifactIRenderer maintenance rule:
// Do not rewrite the existing D3D12-specific path by guesswork.
// Do not replace this renderer with a Qt-only implementation.
// Extend backends carefully while preserving the current Diligent/D3D12 architecture.
#include <array>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <mutex>
#include <functional>
#include <QImage>
#include <QElapsedTimer>
#include <QDebug>
#include <QtConcurrent>
#include <QTransform>
#include <QMatrix4x4>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Query.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Common/interface/Float16.hpp>
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
import Artifact.Render.PrimitiveRenderer3D;
import Artifact.Render.Config;
import Graphics.ParticleRenderer;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentImmediateSubmitter;
import Artifact.Render.RenderCommandBuffer;

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
 public:
  DiligentDeviceManager deviceManager_;
  ShaderManager shaderManager_;
  PrimitiveRenderer2D primitiveRenderer_;
  PrimitiveRenderer3D primitiveRenderer3D_;
  std::unique_ptr<ArtifactCore::IRayTracingManager> rayTracingManager_;
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
  std::unique_ptr<ArtifactCore::ParticleRenderer> particleRenderer_;

  mutable DiligentImmediateSubmitter submitter_;
  mutable RenderCommandBuffer cmdBuf_;

  RefCntAutoPtr<ITexture> m_layerRT;
  Uint32 m_layerRTWidth = 0;
  Uint32 m_layerRTHeight = 0;
  mutable RefCntAutoPtr<ITexture> m_readbackStagingTex;
  mutable TEXTURE_FORMAT m_readbackStagingFormat = TEX_FORMAT_UNKNOWN;
  mutable RefCntAutoPtr<IFence> m_readbackFence;
  mutable Uint32 m_readbackStagingWidth = 0;
  mutable Uint32 m_readbackStagingHeight = 0;
  mutable Uint64 m_readbackFenceValue = 0;
  mutable std::mutex m_readbackMutex;
  QWidget* widget_ = nullptr;

  bool m_initialized = false;
  bool m_frameQueryInitialized = false;
  double m_lastGpuFrameTimeMs = 0.0;
  Uint32 m_frameQueryIndex = 0;
  static constexpr Uint32 FrameQueryCount = 2;
  std::array<RefCntAutoPtr<IQuery>, FrameQueryCount> m_frameQueries;
  int m_offlineWidth  = 0;
  int m_offlineHeight = 0;
  float m_viewportWidth  = 0.0f;
  float m_viewportHeight = 0.0f;

  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };

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
   bool isInitialized() const { return m_initialized; }

   void clear();
  void setClearColor(const FloatColor& color);
  FloatColor getClearColor() const { return clearColor_; }
  void flushAndWait();
  void flush();
  void destroy();

  // Viewport
  void setViewportSize(float w, float h) { primitiveRenderer_.setViewportSize(w, h); m_viewportWidth = w; m_viewportHeight = h; }
  void setCanvasSize(float w, float h)   { primitiveRenderer_.setCanvasSize(w, h); }
  void setPan(float x, float y)          { primitiveRenderer_.setPan(x, y); }
  void getPan(float& x, float& y) const  { primitiveRenderer_.getPan(x, y); }
  void setZoom(float zoom)               { primitiveRenderer_.setZoom(zoom); }
  float getZoom() const                  { return primitiveRenderer_.getZoom(); }
  void panBy(float dx, float dy)         { primitiveRenderer_.panBy(dx, dy); }
  void resetView()                       { primitiveRenderer_.resetView(); }
  void fitToViewport(float margin)       { primitiveRenderer_.fitToViewport(margin); }
  void fillToViewport(float margin)      { primitiveRenderer_.fillToViewport(margin); }
  void setViewMatrix(const QMatrix4x4& view) { primitiveRenderer_.setViewMatrix(view); primitiveRenderer3D_.setViewMatrix(view); }
  void setProjectionMatrix(const QMatrix4x4& proj) { primitiveRenderer_.setProjectionMatrix(proj); primitiveRenderer3D_.setProjectionMatrix(proj); }
  void setUseExternalMatrices(bool use)  { primitiveRenderer_.setUseExternalMatrices(use); }
  void setGizmoCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj) { primitiveRenderer3D_.setCameraMatrices(view, proj); }
  void resetGizmoCameraMatrices() { primitiveRenderer3D_.resetMatrices(); }
  void set3DCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj) { primitiveRenderer3D_.setCameraMatrices(view, proj); }
  void reset3DCameraMatrices() { primitiveRenderer3D_.resetMatrices(); }
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
  void drawPolyline(const std::vector<Detail::float2>& points, const FloatColor& color, float thickness)
  {
    if (points.size() < 2) return;
    for (size_t i = 0; i < points.size() - 1; ++i) {
      primitiveRenderer_.drawThickLineLocal(points[i], points[i+1], thickness, color);
    }
  }
  void drawQuadLocal(float2 p0, float2 p1, float2 p2, float2 p3, const FloatColor& color)
  { primitiveRenderer_.drawQuadLocal(p0, p1, p2, p3, color); }
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
  void drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform,
                                const FloatColor& color, float opacity)
  { primitiveRenderer_.drawSolidRectTransformed(x, y, w, h, transform, color, opacity); }
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

  void drawParticles(const Artifact::ParticleRenderData& data) {
    if (!particleRenderer_) {
      if (!deviceManager_.device()) return;
      // Lazy initialization of particle renderer
      if (!gpuContext_) {
        gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(deviceManager_.device(), deviceManager_.immediateContext());
      }
      particleRenderer_ = std::make_unique<ArtifactCore::ParticleRenderer>(*gpuContext_);
      particleRenderer_->initialize(100000); // Support up to 100k particles
    }

    auto ctx = deviceManager_.immediateContext();
    if (!ctx) return;

    if (m_viewportWidth <= 0.0f || m_viewportHeight <= 0.0f) return;

    // Build an orthographic View + Projection that replicates PrimitiveRenderer2D's
    // internal pan/zoom/viewport transform.
    //
    // The particle VS uses  mul(pos, ViewMatrix)  and  mul(viewPos, ProjMatrix),
    // which is the HLSL row-vector convention with a column_major cbuffer.
    // QMatrix4x4 stores data in column-major (column-vector convention), so
    // passing constData() directly means the HLSL matrix equals M_qt, and
    //   mul(v, M_qt)  ≡  v * M_qt  ≠  M_qt * v
    // Every matrix must therefore be transposed before upload.
    float panX = 0.0f, panY = 0.0f;
    primitiveRenderer_.getPan(panX, panY);
    const float zoom = primitiveRenderer_.getZoom();

    // View: canvas space → viewport pixel space  (translate then scale)
    QMatrix4x4 view;
    view.translate(panX, panY, 0.0f);
    view.scale(zoom, zoom, 1.0f);

    // Proj: orthographic, viewport pixels → NDC, Y-axis flipped (screen Y-down)
    QMatrix4x4 proj;
    proj.ortho(0.0f, m_viewportWidth, m_viewportHeight, 0.0f, -1.0f, 1.0f);

    // Transpose for HLSL  mul(vector, matrix)  row-vector convention
    const QMatrix4x4 viewT = view.transposed();
    const QMatrix4x4 projT = proj.transposed();
    particleRenderer_->setViewMatrix(viewT.constData());
    particleRenderer_->setProjectionMatrix(projT.constData());

    particleRenderer_->updateBuffer(data);
    particleRenderer_->prepare(ctx);
    particleRenderer_->draw(ctx, data.particles.size());
  }
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
   QElapsedTimer timer;
   timer.start();
   deviceManager_.initialize(widget);
   qInfo() << "[ArtifactIRenderer][Init] deviceManager.initialize ms=" << timer.elapsed();

   if (!deviceManager_.isInitialized()) {
    qWarning() << "[ArtifactIRenderer] initialize() failed: deviceManager not initialized"
               << "widget=" << widget << "size=" << (widget ? widget->size() : QSize());
    return;
   }

  timer.restart();
  shaderManager_.initialize(deviceManager_.device(), RenderConfig::MainRTVFormat);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();
  qInfo() << "[ArtifactIRenderer][Init] shaders+psos ms=" << timer.elapsed();

  timer.restart();
  rayTracingManager_ = ArtifactCore::createRayTracingManager();
  rayTracingManager_->initialize(deviceManager_.device());
  qInfo() << "[ArtifactIRenderer][Init] rayTracingManager ms=" << timer.elapsed();

  timer.restart();
  primitiveRenderer_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  primitiveRenderer_.setPSOs(shaderManager_);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                deviceManager_.swapChain());
  submitter_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  submitter_.setPSOs(shaderManager_);
  primitiveRenderer_.setCommandBuffer(&cmdBuf_);
  qInfo() << "[ArtifactIRenderer][Init] primitiveRenderer2D ms=" << timer.elapsed();

  timer.restart();
  primitiveRenderer3D_.createBuffers(deviceManager_.device());
  primitiveRenderer3D_.setPSOs(shaderManager_);
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
                                  deviceManager_.swapChain());
  qInfo() << "[ArtifactIRenderer][Init] primitiveRenderer3D ms=" << timer.elapsed();

  m_initialized = true;
 }

 void ArtifactIRenderer::Impl::initializeHeadless(int width, int height)
 {
  m_offlineWidth  = width;
  m_offlineHeight = height;

  deviceManager_.initializeHeadless();
  if (!deviceManager_.isInitialized()) return;

  shaderManager_.initialize(deviceManager_.device(), RenderConfig::MainRTVFormat);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();

  rayTracingManager_ = ArtifactCore::createRayTracingManager();
  rayTracingManager_->initialize(deviceManager_.device());

  primitiveRenderer_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  primitiveRenderer_.setPSOs(shaderManager_);
  submitter_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  submitter_.setPSOs(shaderManager_);
  primitiveRenderer_.setCommandBuffer(&cmdBuf_);

  primitiveRenderer3D_.createBuffers(deviceManager_.device());
  primitiveRenderer3D_.setPSOs(shaderManager_);

  TextureDesc TexDesc;
  TexDesc.Name      = "OfflineRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = static_cast<Uint32>(width);
  TexDesc.Height    = static_cast<Uint32>(height);
  TexDesc.MipLevels = 1;
  TexDesc.Format    = RenderConfig::MainRTVFormat;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);

  auto* rtv = m_layerRT ? m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : nullptr;
  primitiveRenderer_.setOverrideRTV(rtv);
  primitiveRenderer3D_.setOverrideRTV(rtv);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(), nullptr);
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext());
  primitiveRenderer_.setViewportSize(float(width), float(height));
  m_viewportWidth  = float(width);
  m_viewportHeight = float(height);
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
  std::lock_guard<std::mutex> guard(m_readbackMutex);

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

  const TEXTURE_FORMAT srcFormat = srcTex->GetDesc().Format;
  const bool useFloatReadback = (srcFormat == TEX_FORMAT_RGBA16_FLOAT);
  const TEXTURE_FORMAT stagingFormat =
      useFloatReadback ? TEX_FORMAT_RGBA16_FLOAT : TEX_FORMAT_RGBA8_UNORM;

  // Staging texture mirrors the source format so we can either memcpy raw bytes
  // for 8-bit sources or unpack half-floats for the HDR swap chain path.
  if (!m_readbackStagingTex ||
      m_readbackStagingWidth != srcWidth ||
      m_readbackStagingHeight != srcHeight ||
      m_readbackStagingFormat != stagingFormat)
  {
    TextureDesc stagDesc;
    stagDesc.Name           = "ReadbackStagingTexture";
    stagDesc.Type           = RESOURCE_DIM_TEX_2D;
    stagDesc.Width          = srcWidth;
    stagDesc.Height         = srcHeight;
    stagDesc.MipLevels      = 1;
    stagDesc.Format         = stagingFormat;
    stagDesc.Usage          = USAGE_STAGING;
    stagDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagDesc.BindFlags      = BIND_NONE;
    device->CreateTexture(stagDesc, nullptr, &m_readbackStagingTex);
    if (!m_readbackStagingTex) return {};
    m_readbackStagingWidth = srcWidth;
    m_readbackStagingHeight = srcHeight;
    m_readbackStagingFormat = stagingFormat;
   }

  if (!m_readbackFence) {
   FenceDesc fDesc;
   fDesc.Name = "ReadbackFence";
   fDesc.Type = FENCE_TYPE_GENERAL;
   device->CreateFence(fDesc, &m_readbackFence);
   if (!m_readbackFence) return {};
   m_readbackFenceValue = 0;
  }

  // Flush any pending draw packets before reading back.
  submitter_.submit(cmdBuf_, ctx);

  // Transition both textures to the required states and issue the copy.
  // Unbind the render target first so Vulkan doesn't complain about copying
  // from a texture that is still attached as an RTV.
  ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture              = srcTex;
  copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copyAttribs.pDstTexture              = m_readbackStagingTex;
  copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  ctx->CopyTexture(copyAttribs);

  // Reused fence: CPU waits until GPU copy completion before mapping.
  const Uint64 waitValue = ++m_readbackFenceValue;
  ctx->EnqueueSignal(m_readbackFence, waitValue);
  ctx->Flush();
  m_readbackFence->Wait(waitValue);

  // Map the staging texture. The fence wait above guarantees the GPU copy has
  // finished, so DO_NOT_WAIT is safe and avoids Vulkan backend warnings.
  MappedTextureSubresource mapped = {};
  ctx->MapTextureSubresource(m_readbackStagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
  if (!mapped.pData) return {};

  QImage result(static_cast<int>(srcWidth), static_cast<int>(srcHeight),
                QImage::Format_RGBA8888);
  const size_t copyRowBytes = static_cast<size_t>(srcWidth) * 4u;
  const size_t sourceRowBytes =
      useFloatReadback ? static_cast<size_t>(srcWidth) * 8u : copyRowBytes;
  if (mapped.Stride < sourceRowBytes) {
   ctx->UnmapTextureSubresource(m_readbackStagingTex, 0, 0);
   return {};
  }
  if (!useFloatReadback) {
   const auto* srcRow = static_cast<const uint8_t*>(mapped.pData);
   for (Uint32 row = 0; row < srcHeight; ++row) {
    std::memcpy(result.scanLine(static_cast<int>(row)), srcRow, copyRowBytes);
    srcRow += mapped.Stride;
   }
  } else {
   const auto* srcRow = static_cast<const uint16_t*>(mapped.pData);
   for (Uint32 row = 0; row < srcHeight; ++row) {
    auto* dst = result.scanLine(static_cast<int>(row));
    const auto* srcHalf = srcRow;
    for (Uint32 x = 0; x < srcWidth; ++x) {
     // Half-float -> 8-bit sRGB for the HDR swap chain path.
     const float r = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 0]), 0.0f, 1.0f);
     const float g = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 1]), 0.0f, 1.0f);
     const float b = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 2]), 0.0f, 1.0f);
     const float a = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 3]), 0.0f, 1.0f);
     dst[x * 4 + 0] = static_cast<uint8_t>(std::pow(r, 1.0f / 2.2f) * 255.0f + 0.5f);
     dst[x * 4 + 1] = static_cast<uint8_t>(std::pow(g, 1.0f / 2.2f) * 255.0f + 0.5f);
     dst[x * 4 + 2] = static_cast<uint8_t>(std::pow(b, 1.0f / 2.2f) * 255.0f + 0.5f);
     dst[x * 4 + 3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
    }
    srcRow = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(srcRow) + mapped.Stride);
   }
  }
  ctx->UnmapTextureSubresource(m_readbackStagingTex, 0, 0);
  return result;
 }

 void ArtifactIRenderer::Impl::readbackToImageAsync(ReadbackCallback callback) const
 {
  if (!deviceManager_.device() || !deviceManager_.immediateContext()) {
    if (callback) callback(QImage());
    return;
  }

  auto* ctx = deviceManager_.immediateContext();
  auto* device = deviceManager_.device();

  // Get swap chain back buffer info
  RefCntAutoPtr<ITexture> srcTex;
  Uint32 srcWidth = 0, srcHeight = 0;

  if (auto* sc = deviceManager_.swapChain()) {
   if (auto* rtv = sc->GetCurrentBackBufferRTV()) {
    srcTex = rtv->GetTexture();
    if (srcTex) {
     const auto& desc = srcTex->GetDesc();
     srcWidth  = desc.Width;
     srcHeight = desc.Height;
    }
   }
  }

  if (!srcTex || srcWidth == 0 || srcHeight == 0) {
    if (callback) callback(QImage());
    return;
  }

  const TEXTURE_FORMAT srcFormat = srcTex->GetDesc().Format;
  const bool useFloatReadback = (srcFormat == TEX_FORMAT_RGBA16_FLOAT);
  const TEXTURE_FORMAT stagingFormat =
      useFloatReadback ? TEX_FORMAT_RGBA16_FLOAT : TEX_FORMAT_RGBA8_UNORM;

  // Create/recreate staging texture if needed
  RefCntAutoPtr<ITexture> stagingTex;
  {
    TextureDesc stagDesc;
    stagDesc.Name           = "AsyncReadbackStaging";
    stagDesc.Type           = RESOURCE_DIM_TEX_2D;
    stagDesc.Width          = srcWidth;
    stagDesc.Height         = srcHeight;
    stagDesc.MipLevels      = 1;
    stagDesc.Format         = stagingFormat;
    stagDesc.Usage          = USAGE_STAGING;
    stagDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagDesc.BindFlags      = BIND_NONE;
    device->CreateTexture(stagDesc, nullptr, &stagingTex);
    if (!stagingTex) {
      if (callback) callback(QImage());
      return;
    }
  }

  // Create/reuse fence
  RefCntAutoPtr<IFence> fence;
  {
    FenceDesc fDesc;
    fDesc.Name = "AsyncReadbackFence";
    fDesc.Type = FENCE_TYPE_GENERAL;
    device->CreateFence(fDesc, &fence);
    if (!fence) {
      if (callback) callback(QImage());
      return;
    }
  }

  // Unbind render target, then copy
  ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture              = srcTex;
  copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copyAttribs.pDstTexture              = stagingTex;
  copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  ctx->CopyTexture(copyAttribs);

  // Signal fence and flush (non-blocking)
  const Uint64 waitValue = 1;
  ctx->EnqueueSignal(fence, waitValue);
  ctx->Flush();

  // Capture data for background thread
  const Uint32 w = srcWidth;
  const Uint32 h = srcHeight;
  const bool floatReadback = useFloatReadback;

  // Run fence wait + pixel conversion in background thread
  QtConcurrent::run([fence, stagingTex, ctx, w, h, floatReadback, cb = std::move(callback)]() mutable {
    // Wait for GPU copy to complete
    fence->Wait(waitValue);

    // Map staging texture
    MappedTextureSubresource mapped = {};
    ctx->MapTextureSubresource(stagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (!mapped.pData) {
      if (cb) cb(QImage());
      return;
    }

    // Convert to QImage
    QImage result(static_cast<int>(w), static_cast<int>(h), QImage::Format_RGBA8888);
    const size_t copyRowBytes = static_cast<size_t>(w) * 4u;
    const size_t sourceRowBytes =
        floatReadback ? static_cast<size_t>(w) * 8u : copyRowBytes;

    if (mapped.Stride < sourceRowBytes) {
      ctx->UnmapTextureSubresource(stagingTex, 0, 0);
      if (cb) cb(QImage());
      return;
    }

    if (!floatReadback) {
      const auto* srcRow = static_cast<const uint8_t*>(mapped.pData);
      for (Uint32 row = 0; row < h; ++row) {
        std::memcpy(result.scanLine(static_cast<int>(row)), srcRow, copyRowBytes);
        srcRow += mapped.Stride;
      }
    } else {
      const auto* srcRow = static_cast<const uint16_t*>(mapped.pData);
      for (Uint32 row = 0; row < h; ++row) {
        auto* dst = result.scanLine(static_cast<int>(row));
        const auto* srcHalf = srcRow;
        for (Uint32 x = 0; x < w; ++x) {
          const float r = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 0]), 0.0f, 1.0f);
          const float g = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 1]), 0.0f, 1.0f);
          const float b = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 2]), 0.0f, 1.0f);
          const float a = std::clamp(Float16::HalfBitsToFloat(srcHalf[x * 4 + 3]), 0.0f, 1.0f);
          dst[x * 4 + 0] = static_cast<uint8_t>(std::pow(r, 1.0f / 2.2f) * 255.0f + 0.5f);
          dst[x * 4 + 1] = static_cast<uint8_t>(std::pow(g, 1.0f / 2.2f) * 255.0f + 0.5f);
          dst[x * 4 + 2] = static_cast<uint8_t>(std::pow(b, 1.0f / 2.2f) * 255.0f + 0.5f);
          dst[x * 4 + 3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
        }
        srcRow = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const uint8_t*>(srcRow) + mapped.Stride);
      }
    }

    ctx->UnmapTextureSubresource(stagingTex, 0, 0);

    // Invoke callback on the caller's thread (or thread pool)
    if (cb) cb(result);
  });
 }

 // ---------------------------------------------------------------------------
 // createSwapChain / recreateSwapChain
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::createLayerRT(QWidget* window)
 {
  if (!window || !deviceManager_.device()) return;

  const Uint32 newWidth = static_cast<Uint32>(window->width() * window->devicePixelRatio());
  const Uint32 newHeight = static_cast<Uint32>(window->height() * window->devicePixelRatio());
  if (m_layerRT && m_layerRTWidth == newWidth && m_layerRTHeight == newHeight) {
    return;
  }

  if (m_layerRT) m_layerRT.Release();

  TextureDesc TexDesc;
  TexDesc.Name      = "LayerRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = newWidth;
  TexDesc.Height    = newHeight;
  TexDesc.MipLevels = 1;
  TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);
  m_layerRTWidth = newWidth;
  m_layerRTHeight = newHeight;
 }

 void ArtifactIRenderer::Impl::createSwapChain(QWidget* window)
 {
  if (!window || !deviceManager_.device()) return;

  widget_ = window;
  deviceManager_.createSwapChain(window);

  if (!m_initialized) {
   shaderManager_.initialize(deviceManager_.device(), RenderConfig::MainRTVFormat);
   shaderManager_.createShaders();
   shaderManager_.createPSOs();
   primitiveRenderer_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
   primitiveRenderer_.setPSOs(shaderManager_);
   primitiveRenderer3D_.createBuffers(deviceManager_.device());
   primitiveRenderer3D_.setPSOs(shaderManager_);
   submitter_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
   submitter_.setPSOs(shaderManager_);
   primitiveRenderer_.setCommandBuffer(&cmdBuf_);
   m_initialized = true;
  }

  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                 deviceManager_.swapChain());
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
                                  deviceManager_.swapChain());
  createLayerRT(window);
 }

 void ArtifactIRenderer::Impl::recreateSwapChain(QWidget* widget)
 {
  if (!widget || !deviceManager_.device()) return;

  const int newWidth  = static_cast<int>(widget->width()  * widget->devicePixelRatio());
  const int newHeight = static_cast<int>(widget->height() * widget->devicePixelRatio());
  if (newWidth <= 0 || newHeight <= 0) return;

  // If swapchain doesn't exist yet (deferred from 0×0 init), create from scratch
  if (!deviceManager_.swapChain()) {
    qDebug() << "[ArtifactIRenderer] recreateSwapChain: no swapchain — calling createSwapChain";
    createSwapChain(widget);
    return;
  }

  deviceManager_.recreateSwapChain(widget);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                 deviceManager_.swapChain());
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
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
  primitiveRenderer_.clear(deviceManager_.immediateContext(), clearColor_);
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
  submitter_.destroy();
  cmdBuf_.reset();
  m_readbackStagingTex= nullptr;
  m_readbackStagingFormat = TEX_FORMAT_UNKNOWN;
  m_readbackFence = nullptr;
  m_readbackStagingWidth = 0;
  m_readbackStagingHeight = 0;
  m_readbackFenceValue = 0;
  m_layerRT = nullptr;
  for (auto& query : m_frameQueries) query = nullptr;
  primitiveRenderer_.destroy();
  primitiveRenderer3D_.destroy();
  shaderManager_.destroy();
  deviceManager_.destroy();
  widget_                = nullptr;
  m_initialized          = false;
  m_frameQueryInitialized = false;
 }

 void ArtifactIRenderer::Impl::present()
  {
   if (auto sc = deviceManager_.swapChain())
   {
    submitter_.submit(cmdBuf_, deviceManager_.immediateContext());
    try {
     sc->Present();
    } catch (const std::exception& ex) {
     const QString msg = QString::fromLocal8Bit(ex.what());
     qWarning() << "[ArtifactIRenderer] present() failed:" << msg;

     const bool surfaceLost =
         msg.contains(QStringLiteral("ERROR_SURFACE_LOST_KHR"), Qt::CaseInsensitive) ||
         msg.contains(QStringLiteral("surface lost"), Qt::CaseInsensitive) ||
         msg.contains(QStringLiteral("Failed to query physical device surface capabilities"), Qt::CaseInsensitive);

     if (surfaceLost && widget_ && deviceManager_.device()) {
      qWarning() << "[ArtifactIRenderer] attempting swapchain recreation after surface loss";
      try {
       recreateSwapChain(widget_);
      } catch (const std::exception& recreateEx) {
       qWarning() << "[ArtifactIRenderer] swapchain recreation failed:"
                  << QString::fromLocal8Bit(recreateEx.what());
      }
     }
    }
   }
   else {
    static bool warned = false;
    if (!warned) {
     warned = true;
     qWarning() << "[ArtifactIRenderer] present() skipped: swapChain is null";
    }
   }
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
  bool ArtifactIRenderer::isInitialized() const { return impl_->isInitialized(); }
  bool ArtifactIRenderer::hasSwapChain() const { return impl_->deviceManager_.swapChain() != nullptr; }

 QImage ArtifactIRenderer::readbackToImage() const { return impl_->readbackToImage(); }
 void ArtifactIRenderer::readbackToImageAsync(ReadbackCallback callback) const {
  impl_->readbackToImageAsync(std::move(callback));
 }

 void ArtifactIRenderer::present()
 {
  impl_->present();
 }

 void ArtifactIRenderer::setClearColor(const FloatColor& color) { impl_->setClearColor(color); }
 FloatColor ArtifactIRenderer::getClearColor() const { return impl_->getClearColor(); }
 void ArtifactIRenderer::setViewportSize(float w, float h) { impl_->setViewportSize(w, h); }
 void ArtifactIRenderer::setCanvasSize(float w, float h)        { impl_->setCanvasSize(w, h); }
 void ArtifactIRenderer::setPan(float x, float y)               { impl_->setPan(x, y); }
 void ArtifactIRenderer::getPan(float& x, float& y) const       { impl_->getPan(x, y); }
 void ArtifactIRenderer::setZoom(float zoom)                    { impl_->setZoom(zoom); }
 float ArtifactIRenderer::getZoom() const                       { return impl_->getZoom(); }
 void ArtifactIRenderer::panBy(float dx, float dy)              { impl_->panBy(dx, dy); }
 void ArtifactIRenderer::resetView()                            { impl_->resetView(); }
 void ArtifactIRenderer::fitToViewport(float margin)            { impl_->fitToViewport(margin); }
 void ArtifactIRenderer::fillToViewport(float margin)            { impl_->fillToViewport(margin); }
 void ArtifactIRenderer::setViewMatrix(const QMatrix4x4& view)  { impl_->setViewMatrix(view); }
 void ArtifactIRenderer::setProjectionMatrix(const QMatrix4x4& proj) { impl_->setProjectionMatrix(proj); }
 void ArtifactIRenderer::setUseExternalMatrices(bool use)  { impl_->setUseExternalMatrices(use); }
void ArtifactIRenderer::setGizmoCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj)
{ impl_->setGizmoCameraMatrices(view, proj); }
void ArtifactIRenderer::resetGizmoCameraMatrices()
{ impl_->resetGizmoCameraMatrices(); }
 void ArtifactIRenderer::set3DCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj)
 { impl_->set3DCameraMatrices(view, proj); }
 void ArtifactIRenderer::reset3DCameraMatrices()
 { impl_->reset3DCameraMatrices(); }

 QMatrix4x4 ArtifactIRenderer::getViewMatrix() const { return impl_->primitiveRenderer_.getViewMatrix(); }
 QMatrix4x4 ArtifactIRenderer::getProjectionMatrix() const { return impl_->primitiveRenderer_.getProjectionMatrix(); }

 void ArtifactIRenderer::zoomAroundViewportPoint(Detail::float2 pos, float newZoom)
 { impl_->zoomAroundViewportPoint(pos, newZoom); }
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
 void ArtifactIRenderer::drawPolyline(const std::vector<Detail::float2>& points,
                                      const FloatColor& color, float thickness)
 { impl_->drawPolyline(points, color, thickness); }
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
 void ArtifactIRenderer::drawSprite(float x, float y, float w, float h, Diligent::ITextureView* pSRV, float opacity)
 { impl_->primitiveRenderer_.drawTextureLocal(x, y, w, h, pSRV, opacity); }
 void ArtifactIRenderer::drawSprite(float x, float y, float w, float h, const QImage& image, float opacity)
 { impl_->drawSpriteLocal(x, y, w, h, image, opacity); }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity)
 {
  // Direct delegation to primitive renderer for transformed sprite drawing
  impl_->primitiveRenderer_.drawSpriteTransformed(x, y, w, h, transform, image, opacity);
 }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity)
 {
  impl_->primitiveRenderer_.drawSpriteTransformed(x, y, w, h, transform, image, opacity);
 }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, Diligent::ITextureView* texture, float opacity)
 {
  impl_->primitiveRenderer_.drawSpriteTransformed(x, y, w, h, transform, texture, opacity);
 }
 void ArtifactIRenderer::drawMaskedTextureLocal(float x, float y, float w, float h, Diligent::ITextureView* sceneTexture, const QImage& maskImage, float opacity)
 {
  impl_->primitiveRenderer_.drawMaskedTextureLocal(x, y, w, h, sceneTexture, maskImage, opacity);
 }
 void ArtifactIRenderer::drawRectLocal(float x, float y, float w, float h, const FloatColor& color, float opacity)
 { impl_->drawRectLocal(x, y, w, h, color, opacity); }
 void ArtifactIRenderer::drawSolidRectTransformed(float x, float y, float w, float h, const QTransform& transform, const FloatColor& color, float opacity)
 { impl_->drawSolidRectTransformed(x, y, w, h, transform, color, opacity); }
 void ArtifactIRenderer::drawSolidRectTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const FloatColor& color, float opacity)
 { impl_->primitiveRenderer_.drawSolidRectTransformed(x, y, w, h, transform, color, opacity); }
 void ArtifactIRenderer::drawRectOutlineLocal(float x, float y, float w, float h, const FloatColor& color)
 { impl_->drawRectOutlineLocal(x, y, w, h, color); }
void ArtifactIRenderer::drawThickLineLocal(Detail::float2 p1, Detail::float2 p2,
                                           float thickness, const FloatColor& color)
{ impl_->drawThickLineLocal(toDiligentFloat2(p1), toDiligentFloat2(p2), thickness, color); }
void ArtifactIRenderer::drawQuadLocal(Detail::float2 p0, Detail::float2 p1,
                                      Detail::float2 p2, Detail::float2 p3,
                                      const FloatColor& color)
{ impl_->primitiveRenderer_.drawQuadLocal(toDiligentFloat2(p0), toDiligentFloat2(p1), toDiligentFloat2(p2), toDiligentFloat2(p3), color); }
void ArtifactIRenderer::drawDotLineLocal(Detail::float2 p1, Detail::float2 p2,
                                          float thickness, float spacing, const FloatColor& color)
{ impl_->drawDotLineLocal(toDiligentFloat2(p1), toDiligentFloat2(p2), thickness, spacing, color); }
void ArtifactIRenderer::drawDashedLineLocal(Detail::float2 p1, Detail::float2 p2,
                                            float thickness, float dashLength, float gapLength, const FloatColor& color)
{ impl_->primitiveRenderer_.drawDashedLineLocal(toDiligentFloat2(p1), toDiligentFloat2(p2), thickness, dashLength, gapLength, color); }
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
 void ArtifactIRenderer::drawCircle(float x, float y, float radius, const FloatColor& color, float thickness, bool fill)
 { impl_->primitiveRenderer_.drawCircle(x, y, radius, color, thickness, fill); }
 void ArtifactIRenderer::drawCrosshair(float x, float y, float size, const FloatColor& color)
 { impl_->primitiveRenderer_.drawCrosshair(x, y, size, color); }
void ArtifactIRenderer::drawCheckerboard(float x, float y, float w, float h,
                                         float tileSize, const FloatColor& c1, const FloatColor& c2)
{ impl_->drawCheckerboard(x, y, w, h, tileSize, c1, c2); }
void ArtifactIRenderer::drawGrid(float x, float y, float w, float h,
                                 float spacing, float thickness, const FloatColor& color)
{ impl_->drawGrid(x, y, w, h, spacing, thickness, color); }
void ArtifactIRenderer::drawParticles(const Artifact::ParticleRenderData& data) { impl_->drawParticles(data); }
void ArtifactIRenderer::drawGizmoLine(Detail::float3 start, Detail::float3 end, const FloatColor& color, float thickness)
{ impl_->primitiveRenderer3D_.draw3DLine({start.x, start.y, start.z}, {end.x, end.y, end.z}, color, thickness); }
void ArtifactIRenderer::drawGizmoArrow(Detail::float3 start, Detail::float3 end, const FloatColor& color, float size)
{ impl_->primitiveRenderer3D_.draw3DArrow({start.x, start.y, start.z}, {end.x, end.y, end.z}, color, size); }
void ArtifactIRenderer::drawGizmoRing(Detail::float3 center, Detail::float3 normal, float radius, const FloatColor& color, float thickness)
{ impl_->primitiveRenderer3D_.draw3DCircle({center.x, center.y, center.z}, {normal.x, normal.y, normal.z}, radius, color, thickness); }
void ArtifactIRenderer::drawGizmoTorus(Detail::float3 center, Detail::float3 normal, float majorRadius, float minorRadius, const FloatColor& color)
{ impl_->primitiveRenderer3D_.draw3DTorus({center.x, center.y, center.z}, {normal.x, normal.y, normal.z}, majorRadius, minorRadius, color); }
void ArtifactIRenderer::drawGizmoCube(Detail::float3 center, float halfExtent, const FloatColor& color)
{ impl_->primitiveRenderer3D_.draw3DCube({center.x, center.y, center.z}, halfExtent, color); }
void ArtifactIRenderer::flushGizmo3D()
{ impl_->primitiveRenderer3D_.flushGizmo3D(); }
void ArtifactIRenderer::draw3DLine(Detail::float3 start, Detail::float3 end, const FloatColor& color, float thickness)
{ drawGizmoLine(start, end, color, thickness); }
void ArtifactIRenderer::draw3DArrow(Detail::float3 start, Detail::float3 end, const FloatColor& color, float size)
{ drawGizmoArrow(start, end, color, size); }
void ArtifactIRenderer::draw3DCircle(Detail::float3 center, Detail::float3 normal, float radius, const FloatColor& color, float thickness)
{ drawGizmoRing(center, normal, radius, color, thickness); }
void ArtifactIRenderer::draw3DQuad(Detail::float3 v0, Detail::float3 v1, Detail::float3 v2, Detail::float3 v3, const FloatColor& color)
{ impl_->primitiveRenderer3D_.draw3DQuad({v0.x, v0.y, v0.z}, {v1.x, v1.y, v1.z}, {v2.x, v2.y, v2.z}, {v3.x, v3.y, v3.z}, color); }
void ArtifactIRenderer::setUpscaleConfig(bool, float)    {}
 Diligent::RefCntAutoPtr<Diligent::IRenderDevice> ArtifactIRenderer::device() const
 { return impl_->deviceManager_.device(); }
 Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ArtifactIRenderer::immediateContext() const
 { return impl_->deviceManager_.immediateContext(); }
 Diligent::ITextureView* ArtifactIRenderer::layerTextureView() const
 { return impl_->m_layerRT ? impl_->m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE) : nullptr; }
 Diligent::ITextureView* ArtifactIRenderer::layerRenderTargetView() const
 { return impl_->m_layerRT ? impl_->m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET) : nullptr; }
 ArtifactCore::IRayTracingManager* ArtifactIRenderer::rayTracingManager() const
 { return impl_->rayTracingManager_.get(); }
 void ArtifactIRenderer::setOverrideRTV(Diligent::ITextureView* rtv)
 {
  impl_->primitiveRenderer_.setOverrideRTV(rtv);
  impl_->primitiveRenderer3D_.setOverrideRTV(rtv);
 }

} // namespace Artifact
