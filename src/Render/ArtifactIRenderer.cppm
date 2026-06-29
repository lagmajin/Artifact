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
#include <atomic>
#include <numeric>
#include <memory>
#include <mutex>
#include <functional>
#include <QImage>
#include <QFont>
#include <QColor>
#include <QPainter>
#include <QRectF>
#include <QElapsedTimer>
#include <QDebug>
#include <QtConcurrent>
#include <QTransform>
#include <QMatrix4x4>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <wrl/client.h>
#include <windows.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Query.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/SwapChain.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/Texture.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineD3D12/interface/RenderDeviceD3D12.h>
#include <DiligentCore/Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h>
#include <DiligentCore/Common/interface/Float16.hpp>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Common/interface/BasicMath.hpp>

module Artifact.Render.IRenderer;

import Graphics;
import Graphics.Shader.Set;
import Graphics.Shader.Compile.Task;
import Graphics.Shader.Compute.HLSL.Blend;
import Mesh;
import Material.Material;
import Layer.Blend;
import Graphics.LayerBlendPipeline;
import Core.Scale.Zoom;
import Frame.Debug;
import Image.MultiChannelImage;
import Image.ImageF32x4_RGBA;
import Graphics.MeshRenderer;
import Artifact.Render.DiligentDeviceManager;
import Artifact.Render.ShaderManager;
import Artifact.Render.PrimitiveRenderer2D;
import Artifact.Render.PrimitiveRenderer3D;
import Artifact.Render.Config;
import Graphics.ParticleRenderer;
import Graphics.GPUcomputeContext;
import Artifact.Render.DiligentImmediateSubmitter;
import Artifact.Render.RenderCommandBuffer;
import Core.Light;
import ArtifactCore.Utils.PerformanceProfiler;
import Color.TransferFunction;

namespace Artifact
{
 using namespace Diligent;
 using namespace ArtifactCore;
 using float2 = Diligent::float2;

namespace {
  void reportLiveD3D12Objects(Diligent::IRenderDevice* device)
  {
#if D3D12_SUPPORTED
    if (device == nullptr) {
      return;
    }
    RefCntAutoPtr<IRenderDeviceD3D12> deviceD3D12{device, IID_RenderDeviceD3D12};
    if (!deviceD3D12) {
      return;
    }
    ID3D12Device* d3d12Device = deviceD3D12->GetD3D12Device();
    if (d3d12Device == nullptr) {
      return;
    }
    Microsoft::WRL::ComPtr<ID3D12DebugDevice> debugDevice;
    if (FAILED(d3d12Device->QueryInterface(IID_PPV_ARGS(&debugDevice))) || !debugDevice) {
      return;
    }
    debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
#else
    (void)device;
#endif
  }

  Diligent::float2 toDiligentFloat2(Detail::float2 value) { return { value.x, value.y }; }
  Detail::float2 toDetailFloat2(Diligent::float2 value)   { return { value.x, value.y }; }

  bool nearlyEqual(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) <= epsilon;
  }

  constexpr size_t kArtifactChannelCount =
      static_cast<size_t>(ArtifactIRenderer::ChannelType::Custom) + 1;

  constexpr bool isBaseRgbaChannel(ArtifactIRenderer::ChannelType type)
  {
    return type == ArtifactIRenderer::ChannelType::Red ||
           type == ArtifactIRenderer::ChannelType::Green ||
           type == ArtifactIRenderer::ChannelType::Blue ||
           type == ArtifactIRenderer::ChannelType::Alpha;
  }

  constexpr std::array<bool, kArtifactChannelCount> makeDefaultChannelFlags()
  {
    std::array<bool, kArtifactChannelCount> flags{};
    flags[static_cast<size_t>(ArtifactIRenderer::ChannelType::Red)] = true;
    flags[static_cast<size_t>(ArtifactIRenderer::ChannelType::Green)] = true;
    flags[static_cast<size_t>(ArtifactIRenderer::ChannelType::Blue)] = true;
    flags[static_cast<size_t>(ArtifactIRenderer::ChannelType::Alpha)] = true;
    return flags;
  }

  ArtifactCore::ChannelType toCoreChannel(ArtifactIRenderer::ChannelType type)
  {
    switch (type) {
    case ArtifactIRenderer::ChannelType::Red:        return ArtifactCore::ChannelType::Red;
    case ArtifactIRenderer::ChannelType::Green:      return ArtifactCore::ChannelType::Green;
    case ArtifactIRenderer::ChannelType::Blue:       return ArtifactCore::ChannelType::Blue;
    case ArtifactIRenderer::ChannelType::Alpha:      return ArtifactCore::ChannelType::Alpha;
    case ArtifactIRenderer::ChannelType::Depth:      return ArtifactCore::ChannelType::Depth;
    case ArtifactIRenderer::ChannelType::NormalX:    return ArtifactCore::ChannelType::NormalX;
    case ArtifactIRenderer::ChannelType::NormalY:    return ArtifactCore::ChannelType::NormalY;
    case ArtifactIRenderer::ChannelType::NormalZ:    return ArtifactCore::ChannelType::NormalZ;
    case ArtifactIRenderer::ChannelType::VelocityX:  return ArtifactCore::ChannelType::VelocityX;
    case ArtifactIRenderer::ChannelType::VelocityY:  return ArtifactCore::ChannelType::VelocityY;
    case ArtifactIRenderer::ChannelType::ObjectId:   return ArtifactCore::ChannelType::ObjectId;
    case ArtifactIRenderer::ChannelType::MaterialId: return ArtifactCore::ChannelType::MaterialId;
    case ArtifactIRenderer::ChannelType::Emission:   return ArtifactCore::ChannelType::Emission;
    case ArtifactIRenderer::ChannelType::Custom:     return ArtifactCore::ChannelType::Custom;
    }
    return ArtifactCore::ChannelType::Custom;
  }

  int rgbaOffsetForChannel(ArtifactIRenderer::ChannelType type)
  {
    switch (type) {
    case ArtifactIRenderer::ChannelType::Red:   return 0;
    case ArtifactIRenderer::ChannelType::Green: return 1;
    case ArtifactIRenderer::ChannelType::Blue:  return 2;
    case ArtifactIRenderer::ChannelType::Alpha: return 3;
    default: return -1;
    }
  }

  uint8_t linearToSrgb8(float value)
  {
    constexpr size_t kLutSize = 4096;
    static const std::array<uint8_t, kLutSize> lut = [] {
      std::array<uint8_t, kLutSize> table{};
      for (size_t i = 0; i < kLutSize; ++i) {
        const float x = static_cast<float>(i) / static_cast<float>(kLutSize - 1);
        table[i] = static_cast<uint8_t>(
            ArtifactCore::ColorTransferFunction::encode(
                x, ArtifactCore::TransferFunction::Gamma22) * 255.0f + 0.5f);
      }
      return table;
    }();

    const float clamped = std::clamp(value, 0.0f, 1.0f);
    const size_t index = static_cast<size_t>(clamped * static_cast<float>(kLutSize - 1) + 0.5f);
    return lut[std::min(index, kLutSize - 1)];
  }

  float polygonSignedArea(const std::vector<Detail::float2>& points)
  {
    if (points.size() < 3) {
      return 0.0f;
    }
    double area = 0.0;
    for (size_t i = 0; i < points.size(); ++i) {
      const auto& a = points[i];
      const auto& b = points[(i + 1) % points.size()];
      area += (static_cast<double>(a.x) * static_cast<double>(b.y)) -
              (static_cast<double>(b.x) * static_cast<double>(a.y));
    }
    return static_cast<float>(area * 0.5);
  }

  float cross2d(const Detail::float2& a, const Detail::float2& b, const Detail::float2& c)
  {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
  }

  bool pointInTriangle(const Detail::float2& p, const Detail::float2& a,
                       const Detail::float2& b, const Detail::float2& c)
  {
    const float area1 = cross2d(p, a, b);
    const float area2 = cross2d(p, b, c);
    const float area3 = cross2d(p, c, a);
    const bool hasNeg = (area1 < 0.0f) || (area2 < 0.0f) || (area3 < 0.0f);
    const bool hasPos = (area1 > 0.0f) || (area2 > 0.0f) || (area3 > 0.0f);
    return !(hasNeg && hasPos);
  }

  std::vector<std::array<int, 3>> triangulatePolygon(const std::vector<Detail::float2>& points)
  {
    std::vector<std::array<int, 3>> triangles;
    if (points.size() < 3) {
      return triangles;
    }

    std::vector<int> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0);
    const bool ccw = polygonSignedArea(points) >= 0.0f;
    int guard = 0;
    while (indices.size() > 2 && guard < 4096) {
      ++guard;
      bool earFound = false;
      const int m = static_cast<int>(indices.size());
      for (int i = 0; i < m; ++i) {
        const int i0 = indices[(i + m - 1) % m];
        const int i1 = indices[i];
        const int i2 = indices[(i + 1) % m];
        const Detail::float2& a = points[static_cast<size_t>(i0)];
        const Detail::float2& b = points[static_cast<size_t>(i1)];
        const Detail::float2& c = points[static_cast<size_t>(i2)];
        const float cross = cross2d(a, b, c);
        if (ccw ? (cross <= 1e-6f) : (cross >= -1e-6f)) {
          continue;
        }
        bool containsPoint = false;
        for (int j = 0; j < m; ++j) {
          const int idx = indices[j];
          if (idx == i0 || idx == i1 || idx == i2) {
            continue;
          }
          if (pointInTriangle(points[static_cast<size_t>(idx)], a, b, c)) {
            containsPoint = true;
            break;
          }
        }
        if (containsPoint) {
          continue;
        }
        triangles.push_back({i0, i1, i2});
        indices.erase(indices.begin() + i);
        earFound = true;
        break;
      }
      if (!earFound) {
        break;
      }
    }

    if (triangles.empty()) {
      for (size_t i = 1; i + 1 < points.size(); ++i) {
        triangles.push_back({0, static_cast<int>(i), static_cast<int>(i + 1)});
      }
    }
    return triangles;
  }
 }

  bool resolveReadbackSourceTexture(
      ITextureView* sourceView,
      ITexture* layerTexture,
      int offlineWidth,
      int offlineHeight,
      ISwapChain* swapChain,
      ITexture*& outTexture,
      Uint32& outWidth,
      Uint32& outHeight)
  {
    outTexture = nullptr;
    outWidth = 0;
    outHeight = 0;

    if (sourceView) {
      outTexture = sourceView->GetTexture();
      if (!outTexture) {
        return false;
      }
      const auto& desc = outTexture->GetDesc();
      outWidth = desc.Width;
      outHeight = desc.Height;
      return outWidth > 0 && outHeight > 0;
    }

    if (layerTexture && offlineWidth > 0 && offlineHeight > 0) {
      outTexture = layerTexture;
      outWidth = static_cast<Uint32>(offlineWidth);
      outHeight = static_cast<Uint32>(offlineHeight);
      return true;
    }

    if (layerTexture) {
      outTexture = layerTexture;
      const auto& desc = outTexture->GetDesc();
      outWidth = desc.Width;
      outHeight = desc.Height;
      return outWidth > 0 && outHeight > 0;
    }

    if (swapChain) {
      if (auto* rtv = swapChain->GetCurrentBackBufferRTV()) {
        outTexture = rtv->GetTexture();
        if (!outTexture) {
          return false;
        }
        const auto& desc = outTexture->GetDesc();
        outWidth = desc.Width;
        outHeight = desc.Height;
        return outWidth > 0 && outHeight > 0;
      }
    }

    return false;
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
  mutable PrimitiveRenderer3D primitiveRenderer3D_;
  mutable std::map<QString, std::unique_ptr<ArtifactCore::MeshRenderer>> meshRenderers_;
  mutable std::map<QString, std::pair<size_t, size_t>> meshRendererGeometry_;
  std::unique_ptr<ArtifactCore::IRayTracingManager> rayTracingManager_;
  std::unique_ptr<ArtifactCore::GpuContext> gpuContext_;
  std::unique_ptr<ArtifactCore::ParticleRenderer> particleRenderer_;
  QString lastParticleDebug_;

  mutable DiligentImmediateSubmitter submitter_;
  mutable RenderCommandBuffer cmdBuf_;
  ArtifactCore::RenderCostStats m_lastFrameCostStats_;
  ArtifactCore::RenderCostStats m_currentFrameCostStats_;

  RefCntAutoPtr<ITexture> m_layerRT;
  RefCntAutoPtr<ITexture> m_layerDepthTex;
  ITextureView* m_overrideColorRTV = nullptr;
  ITextureView* m_overrideDepthDSV = nullptr;
  Uint32 m_layerRTWidth = 0;
  Uint32 m_layerRTHeight = 0;
  bool m_upscaleEnabled = false;
  float m_upscaleSharpness = 1.0f;
  float m_upscaleScale = 1.0f;
  // ---- Readback ring -------------------------------------------------------
  // Sync color readback historically used a single staging texture + fence, which
  // serializes the GPU when two readbacks land in the same frame (e.g. thumbnail +
  // main capture). A 2-slot ring lets the second copy proceed while the CPU maps
  // the previous slot, hiding the fence wait on back-to-back captures.
  struct ReadbackSlot {
   RefCntAutoPtr<ITexture> staging;
   RefCntAutoPtr<IFence>   fence;
   Uint64 signaledValue  = 0;  // value last EnqueueSignal()'d
   Uint64 completedValue = 0;  // value last observed as completed
  };
  struct AsyncReadbackSlot {
   RefCntAutoPtr<ITexture> staging;
   RefCntAutoPtr<IFence>   fence;
   std::shared_ptr<std::atomic_bool> busy = std::make_shared<std::atomic_bool>(false);
   Uint32 width = 0;
   Uint32 height = 0;
   TEXTURE_FORMAT format = TEX_FORMAT_UNKNOWN;
  };
  static constexpr Uint32 kReadbackRingSize = 2;
  static constexpr Uint32 kAsyncReadbackRingSize = 3;
  mutable std::array<ReadbackSlot, kReadbackRingSize> m_readbackRing;
  mutable std::array<AsyncReadbackSlot, kAsyncReadbackRingSize> m_asyncReadbackRing;
  mutable Uint32       m_readbackRingIndex = 0;
  mutable Uint32       m_asyncReadbackRingIndex = 0;
  mutable Uint32       m_readbackStagingWidth = 0;
  mutable Uint32       m_readbackStagingHeight = 0;
  mutable TEXTURE_FORMAT m_readbackStagingFormat = TEX_FORMAT_UNKNOWN;
  mutable Uint32       m_asyncReadbackStagingWidth = 0;
  mutable Uint32       m_asyncReadbackStagingHeight = 0;
  mutable TEXTURE_FORMAT m_asyncReadbackStagingFormat = TEX_FORMAT_UNKNOWN;

  // Depth readback is a different format/usage and historically allocated a fresh
  // staging texture + fence on every call. Caching them removes per-call device
  // traffic from the depth inspection path.
  mutable RefCntAutoPtr<ITexture> m_depthReadbackStaging;
  mutable RefCntAutoPtr<IFence>   m_depthReadbackFence;
  mutable Uint32 m_depthReadbackWidth = 0;
  mutable Uint32 m_depthReadbackHeight = 0;
  mutable Uint64 m_depthReadbackFenceValue = 0;
  mutable std::mutex m_readbackMutex;
  QWidget* widget_ = nullptr;

  bool m_initialized = false;
  bool m_frameQueryInitialized = false;
  bool m_frameQueryActive = false;
  double m_lastGpuFrameTimeMs = 0.0;
  Uint32 m_frameQueryIndex = 0;
  static constexpr Uint32 FrameQueryCount = 2;
  std::array<RefCntAutoPtr<IQuery>, FrameQueryCount> m_frameQueries;
  int m_offlineWidth  = 0;
  int m_offlineHeight = 0;
  float m_viewportWidth  = 0.0f;
  float m_viewportHeight = 0.0f;
  float m_canvasWidth = -1.0f;
  float m_canvasHeight = -1.0f;
  QMatrix4x4 meshViewMatrix_;
  QMatrix4x4 meshProjMatrix_;

  FloatColor clearColor_{ 0.10f, 0.10f, 0.10f, 1.0f };
  bool m_multiChannelEnabled = false;
  std::array<bool, kArtifactChannelCount> m_channelEnabled = makeDefaultChannelFlags();
  quint64 presentAttemptCount_ = 0;
  quint64 presentSuccessCount_ = 0;
  quint64 presentFailureCount_ = 0;
  quint64 presentSkippedCount_ = 0;
  QString lastPresentStatus_ = QStringLiteral("never-presented");
  std::vector<ArtifactCore::Light> m_sceneLights;
  LODManager::DetailLevel detailLevel_ = LODManager::DetailLevel::High;
  bool particle3DCameraActive_ = false;
  QMatrix4x4 particleViewMatrix_;
  QMatrix4x4 particleProjMatrix_;
  bool stereoCameraActive_ = false;
  QMatrix4x4 stereoLeftViewMatrix_;
  QMatrix4x4 stereoRightViewMatrix_;
  QMatrix4x4 stereoProjectionMatrix_;


  void initFrameQueries();
  void createLayerRT(QWidget* window);
  ITextureView* activeColorView() const;
  ITextureView* activeDepthView() const;
  float upscaleRenderScale() const { return m_upscaleEnabled ? m_upscaleScale : 1.0f; }
  void submitQueuedDraws(IDeviceContext* ctx) const
  {
   if (!ctx) {
    return;
   }
    submitter_.submit(cmdBuf_, ctx);
   primitiveRenderer3D_.flushGizmo3D();
  }

  ArtifactCore::MeshRenderer* meshRendererFor(const QString& key)
  {
    auto it = meshRenderers_.find(key);
    if (it != meshRenderers_.end()) {
      return it->second.get();
    }
    if (!gpuContext_) {
      if (!deviceManager_.device() || !deviceManager_.immediateContext()) {
        return nullptr;
      }
      gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(deviceManager_.device(),
                                                               deviceManager_.immediateContext());
    }
    auto renderer = std::make_unique<ArtifactCore::MeshRenderer>(*gpuContext_);
    auto* raw = renderer.get();
    meshRenderers_.emplace(key, std::move(renderer));
    return raw;
  }

  void drawMesh(const QString& cacheKey, const ArtifactCore::Mesh& mesh,
                const ArtifactCore::Material& material,
                const QMatrix4x4& modelMatrix, float opacity)
  {
    auto* renderer = meshRendererFor(cacheKey);
    if (!renderer) {
      return;
    }

    const auto data = mesh.generateRenderData();
    if (data.positions.isEmpty()) {
      return;
    }

    const size_t vertexCount = static_cast<size_t>(data.positions.size());
    const size_t indexCount = static_cast<size_t>(data.indices.size());
    const auto geometryIt = meshRendererGeometry_.find(cacheKey);
    if (geometryIt == meshRendererGeometry_.end() ||
        geometryIt->second.first != vertexCount ||
        geometryIt->second.second != indexCount) {
      renderer->setFrameCostStats(&m_currentFrameCostStats_);
      renderer->initialize(1, vertexCount, indexCount);
      meshRendererGeometry_[cacheKey] = {vertexCount, indexCount};
    }

    std::vector<float> positions;
    positions.reserve(vertexCount * 3u);
    for (const auto& p : data.positions) {
      positions.push_back(p.x());
      positions.push_back(p.y());
      positions.push_back(p.z());
    }

    std::vector<float> normals(vertexCount * 3u, 0.0f);
    const bool hasNormals = data.normals.size() == data.positions.size() && !data.normals.isEmpty();
    if (hasNormals) {
      normals.reserve(vertexCount * 3u);
      normals.clear();
      for (const auto& n : data.normals) {
        normals.push_back(n.x());
        normals.push_back(n.y());
        normals.push_back(n.z());
      }
    } else {
      for (size_t i = 0; i < vertexCount; ++i) {
        normals[i * 3u + 0u] = 0.0f;
        normals[i * 3u + 1u] = 0.0f;
        normals[i * 3u + 2u] = 1.0f;
      }
    }

    std::vector<float> uvs(vertexCount * 2u, 0.0f);
    const bool hasUvs = data.uvs.size() == data.positions.size() && !data.uvs.isEmpty();
    if (hasUvs) {
      uvs.reserve(vertexCount * 2u);
      uvs.clear();
      for (const auto& uv : data.uvs) {
        uvs.push_back(uv.x());
        uvs.push_back(uv.y());
      }
    }

    std::vector<uint32_t> indices;
    indices.reserve(indexCount);
    for (const auto idx : data.indices) {
      indices.push_back(static_cast<uint32_t>(idx));
    }

    renderer->setViewMatrix(meshViewMatrix_.constData());
    renderer->setProjectionMatrix(meshProjMatrix_.constData());
    renderer->setBaseColorTexture(material.baseColorTexture().toQString());
    renderer->updateMeshGeometry(positions.data(),
                                 normals.data(),
                                 uvs.data(),
                                 indices.data());

    ArtifactCore::InstanceData instance{};
    const float* modelData = modelMatrix.constData();
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        instance.transform[row * 4 + col] = modelData[col * 4 + row];
      }
    }
    const QColor color = material.baseColor();
    const float alpha = std::clamp(opacity * material.opacity() * color.alphaF(), 0.0f, 1.0f);
    instance.color[0] = color.redF();
    instance.color[1] = color.greenF();
    instance.color[2] = color.blueF();
    instance.color[3] = alpha;
    instance.weight = 1.0f;
    instance.timeOffset = 0.0f;
    renderer->updateInstanceData(&instance, 1);

    auto ctx = deviceManager_.immediateContext();
    if (!ctx) {
      return;
    }
    renderer->prepare(ctx.RawPtr());
    renderer->draw(ctx.RawPtr(), 1);
  }

 public:
  explicit Impl(RefCntAutoPtr<IRenderDevice> device,
                RefCntAutoPtr<IDeviceContext> context, QWidget* widget);
  Impl();
  void present();
  ~Impl();

  void initialize(QWidget* parent);
  void initializeHeadless(int width, int height);
  QImage readbackToImage() const;
  QImage readbackDepthToImage() const;
  QImage readbackChannelToImage(ArtifactIRenderer::ChannelType channel) const;
  QImage readbackTextureViewToImage(ITextureView* textureView) const;
  void createSwapChain(QWidget* widget);
  void recreateSwapChain(QWidget* widget);
  void beginFrameGpuProfiling();
  void endFrameGpuProfiling();
   double lastFrameGpuTimeMs() const;
  quint64 presentAttemptCount() const { return presentAttemptCount_; }
  quint64 presentSuccessCount() const { return presentSuccessCount_; }
  quint64 presentFailureCount() const { return presentFailureCount_; }
  quint64 presentSkippedCount() const { return presentSkippedCount_; }
  QString lastPresentStatus() const { return lastPresentStatus_; }
  void beginFrameCostCapture();
  void endFrameCostCapture();
  ArtifactCore::RenderCostStats frameCostStats() const;
  std::vector<ArtifactCore::FrameDebugPassRecord> frameDebugPasses() const;
   bool isInitialized() const { return m_initialized; }
  void readbackToImageAsync(ArtifactIRenderer::ReadbackCallback callback) const;
  void readbackTextureViewToImageAsync(
      ITextureView* textureView,
      ArtifactIRenderer::ReadbackCallback callback) const;

   void clear();
  void setClearColor(const FloatColor& color);
  void setMultiChannelEnabled(bool enabled);
  bool isMultiChannelEnabled() const { return m_multiChannelEnabled; }
  void setChannelEnabled(ArtifactIRenderer::ChannelType channel, bool enabled);
  bool isChannelEnabled(ArtifactIRenderer::ChannelType channel) const;
  FloatColor getClearColor() const { return clearColor_; }
  void flushAndWait();
  void flush();
  void destroy();

  // Viewport
  void setViewportSize(float w, float h) { primitiveRenderer_.setViewportSize(w, h); m_viewportWidth = w; m_viewportHeight = h; }
  void setViewportRect(float w, float h) {
    setViewportSize(w, h);
    if (auto ctx = deviceManager_.immediateContext()) {
      Viewport VP;
      VP.Width = w;
      VP.Height = h;
      VP.MinDepth = 0.0f;
      VP.MaxDepth = 1.0f;
      VP.TopLeftX = 0.0f;
      VP.TopLeftY = 0.0f;
      ctx->SetViewports(1, &VP, static_cast<Uint32>(w), static_cast<Uint32>(h));
    }
  }
  void unbindColorTargetsForCompute() {
    if (auto ctx = deviceManager_.immediateContext()) {
      ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
  }
  void setCanvasSize(float w, float h) {
    if (nearlyEqual(m_canvasWidth, w) && nearlyEqual(m_canvasHeight, h)) {
      return;
    }
    primitiveRenderer_.setCanvasSize(w, h);
    m_canvasWidth = w;
    m_canvasHeight = h;
  }
  void setPan(float x, float y) {
    float currentX = 0.0f;
    float currentY = 0.0f;
    primitiveRenderer_.getPan(currentX, currentY);
    if (nearlyEqual(currentX, x) && nearlyEqual(currentY, y)) {
      return;
    }
    primitiveRenderer_.setPan(x, y);
  }
  void getPan(float& x, float& y) const  { primitiveRenderer_.getPan(x, y); }
  void setZoom(float zoom) {
    if (nearlyEqual(primitiveRenderer_.getZoom(), zoom)) {
      return;
    }
    primitiveRenderer_.setZoom(zoom);
  }
  float getZoom() const                  { return primitiveRenderer_.getZoom(); }
  void panBy(float dx, float dy)         { primitiveRenderer_.panBy(dx, dy); }
  void resetView()                       { primitiveRenderer_.resetView(); }
  void fitToViewport(float margin)       { primitiveRenderer_.fitToViewport(margin); }

  // LOD
  void setDetailLevel(LODManager::DetailLevel lod) { detailLevel_ = lod; }
  LODManager::DetailLevel detailLevel() const { return detailLevel_; }
  void fillToViewport(float margin)      { primitiveRenderer_.fillToViewport(margin); }
  void setViewMatrix(const QMatrix4x4& view) {
    primitiveRenderer_.setViewMatrix(view);
    primitiveRenderer3D_.setViewMatrix(view);
  }
  void setProjectionMatrix(const QMatrix4x4& proj) {
    primitiveRenderer_.setProjectionMatrix(proj);
    primitiveRenderer3D_.setProjectionMatrix(proj);
  }
  void setUseExternalMatrices(bool use)  { primitiveRenderer_.setUseExternalMatrices(use); }
  void setGizmoCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj) { primitiveRenderer3D_.setCameraMatrices(view, proj); }
  void resetGizmoCameraMatrices() { primitiveRenderer3D_.resetMatrices(); }
  void set3DCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj) {
    primitiveRenderer3D_.setCameraMatrices(view, proj);
    meshViewMatrix_ = view;
    meshProjMatrix_ = proj;
    particle3DCameraActive_ = true;
    particleViewMatrix_ = view;
    particleProjMatrix_ = proj;
  }
  void reset3DCameraMatrices() {
    primitiveRenderer3D_.resetMatrices();
    meshViewMatrix_.setToIdentity();
    meshProjMatrix_.setToIdentity();
    particle3DCameraActive_ = false;
    particleViewMatrix_.setToIdentity();
    particleProjMatrix_.setToIdentity();
  }
  void setStereoCameraMatrices(const QMatrix4x4& leftView,
                               const QMatrix4x4& rightView,
                               const QMatrix4x4& proj) {
    stereoCameraActive_ = true;
    stereoLeftViewMatrix_ = leftView;
    stereoRightViewMatrix_ = rightView;
    stereoProjectionMatrix_ = proj;
    set3DCameraMatrices(leftView, proj);
  }
  void resetStereoCameraMatrices() {
    stereoCameraActive_ = false;
    stereoLeftViewMatrix_.setToIdentity();
    stereoRightViewMatrix_.setToIdentity();
    stereoProjectionMatrix_.setToIdentity();
  }
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
  void drawDashedRectOutline(float x, float y, float w, float h, const FloatColor& color,
                             float thickness, float dashLength, float gapLength)
  {
    const Detail::float2 topLeft{x, y};
    const Detail::float2 topRight{x + w, y};
    const Detail::float2 bottomRight{x + w, y + h};
    const Detail::float2 bottomLeft{x, y + h};
    primitiveRenderer_.drawDashedLineLocal(toDiligentFloat2(topLeft), toDiligentFloat2(topRight),
                                           thickness, dashLength, gapLength, color);
    primitiveRenderer_.drawDashedLineLocal(toDiligentFloat2(topRight), toDiligentFloat2(bottomRight),
                                           thickness, dashLength, gapLength, color);
    primitiveRenderer_.drawDashedLineLocal(toDiligentFloat2(bottomRight), toDiligentFloat2(bottomLeft),
                                           thickness, dashLength, gapLength, color);
    primitiveRenderer_.drawDashedLineLocal(toDiligentFloat2(bottomLeft), toDiligentFloat2(topLeft),
                                           thickness, dashLength, gapLength, color);
  }
  void drawDashedRectOutline(float2 pos, float2 size, const FloatColor& color,
                             float thickness, float dashLength, float gapLength)
  { drawDashedRectOutline(pos.x, pos.y, size.x, size.y, color, thickness, dashLength, gapLength); }
  void drawOverlayPanel(float x, float y, float w, float h, const FloatColor& fillColor,
                        const FloatColor& outlineColor, float opacity)
  {
    primitiveRenderer_.drawRectLocal(x, y, w, h, fillColor, opacity);
    primitiveRenderer_.drawRectOutlineLocal(x, y, w, h, outlineColor);
  }
  void drawOverlayPanel(float2 pos, float2 size, const FloatColor& fillColor,
                        const FloatColor& outlineColor, float opacity)
  { drawOverlayPanel(pos.x, pos.y, size.x, size.y, fillColor, outlineColor, opacity); }
  void drawRoundedPanel(float x, float y, float w, float h, float radius,
                        const FloatColor& fillColor,
                        const FloatColor& outlineColor,
                        float opacity,
                        float outlineThickness)
  {
    if (w <= 0.0f || h <= 0.0f) {
      return;
    }

    radius = std::max(0.0f, std::min(radius, std::min(w, h) * 0.5f));
    if (radius <= 0.01f) {
      drawOverlayPanel(x, y, w, h, fillColor, outlineColor, opacity);
      return;
    }

    const float inset = radius;
    const float innerW = std::max(0.0f, w - inset * 2.0f);
    const float innerH = std::max(0.0f, h - inset * 2.0f);
    if (innerW > 0.0f) {
      primitiveRenderer_.drawRectLocal(x + inset, y, innerW, h, fillColor, opacity);
    }
    if (innerH > 0.0f) {
      primitiveRenderer_.drawRectLocal(x, y + inset, w, innerH, fillColor, opacity);
    }
    primitiveRenderer_.drawCircle(x + inset, y + inset, radius, fillColor, 1.0f, true);
    primitiveRenderer_.drawCircle(x + w - inset, y + inset, radius, fillColor, 1.0f, true);
    primitiveRenderer_.drawCircle(x + w - inset, y + h - inset, radius, fillColor, 1.0f, true);
    primitiveRenderer_.drawCircle(x + inset, y + h - inset, radius, fillColor, 1.0f, true);

    const int segments = std::max(4, static_cast<int>(std::ceil(radius * 0.75f)));
    constexpr float kPi = 3.14159265358979323846f;
    auto drawArc = [this, outlineThickness, &outlineColor, segments, kPi](float cx, float cy,
                                                                          float startDeg,
                                                                          float endDeg,
                                                                          float r) {
      const float degStep = (endDeg - startDeg) / static_cast<float>(segments);
      Detail::float2 prev{
          cx + std::cos(startDeg * kPi / 180.0f) * r,
          cy + std::sin(startDeg * kPi / 180.0f) * r
      };
      for (int i = 1; i <= segments; ++i) {
        const float deg = startDeg + degStep * static_cast<float>(i);
        const Detail::float2 cur{
            cx + std::cos(deg * kPi / 180.0f) * r,
            cy + std::sin(deg * kPi / 180.0f) * r
        };
        primitiveRenderer_.drawThickLineLocal(toDiligentFloat2(prev), toDiligentFloat2(cur),
                                               outlineThickness, outlineColor);
        prev = cur;
      }
    };

    const float left = x;
    const float right = x + w;
    const float top = y;
    const float bottom = y + h;
    const float cxL = left + radius;
    const float cxR = right - radius;
    const float cyT = top + radius;
    const float cyB = bottom - radius;

    primitiveRenderer_.drawThickLineLocal({cxL, top}, {cxR, top}, outlineThickness, outlineColor);
    primitiveRenderer_.drawThickLineLocal({right, cyT}, {right, cyB}, outlineThickness, outlineColor);
    primitiveRenderer_.drawThickLineLocal({cxR, bottom}, {cxL, bottom}, outlineThickness, outlineColor);
    primitiveRenderer_.drawThickLineLocal({left, cyB}, {left, cyT}, outlineThickness, outlineColor);

    drawArc(cxR, cyT, -90.0f,   0.0f, radius);
    drawArc(cxR, cyB,   0.0f,  90.0f, radius);
    drawArc(cxL, cyB,  90.0f, 180.0f, radius);
    drawArc(cxL, cyT, 180.0f, 270.0f, radius);
  }
  void drawRoundedPanel(float2 pos, float2 size, float radius,
                        const FloatColor& fillColor,
                        const FloatColor& outlineColor,
                        float opacity, float outlineThickness)
  { drawRoundedPanel(pos.x, pos.y, size.x, size.y, radius, fillColor, outlineColor, opacity, outlineThickness); }
  void drawSolidLine(Detail::float2 start, Detail::float2 end, const FloatColor& color, float thickness)
  { primitiveRenderer_.drawThickLineLocal(toDiligentFloat2(start), toDiligentFloat2(end), thickness, color); }
  void drawPolyline(const std::vector<Detail::float2>& points, const FloatColor& color, float thickness)
  {
    if (points.size() < 2) return;
    for (size_t i = 0; i < points.size() - 1; ++i) {
      primitiveRenderer_.drawThickLineLocal(toDiligentFloat2(points[i]), toDiligentFloat2(points[i+1]), thickness, color);
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
  void drawSolidPolygonLocal(const std::vector<Detail::float2>& points, const FloatColor& color)
  {
    const auto triangles = triangulatePolygon(points);
    for (const auto& tri : triangles) {
      primitiveRenderer_.drawSolidTriangleLocal(
          toDiligentFloat2(points[static_cast<size_t>(tri[0])]),
          toDiligentFloat2(points[static_cast<size_t>(tri[1])]),
          toDiligentFloat2(points[static_cast<size_t>(tri[2])]),
          color);
    }
  }
  void drawCircle(float x, float y, float radius, const FloatColor& color, float thickness, bool fill)
  { primitiveRenderer_.drawCircle(x, y, radius, color, thickness, fill); }
  void drawCheckerboard(float x, float y, float w, float h,
                        float tileSize, const FloatColor& c1, const FloatColor& c2)
  { primitiveRenderer_.drawCheckerboard(x, y, w, h, tileSize, c1, c2); }
  void drawGrid(float x, float y, float w, float h,
                float spacing, float thickness, const FloatColor& color)
  { primitiveRenderer_.drawGrid(x, y, w, h, spacing, thickness, color); }

  void drawParticles(const ArtifactCore::ParticleRenderData& data) {
    if (data.particles.empty()) {
      lastParticleDebug_ = QStringLiteral(
          "state=empty skipped=empty count=0 path=particle render=none");
      qDebug() << "[ParticleRenderer] drawParticles skipped: empty particle buffer";
      return;
    }

    if (!particleRenderer_) {
      if (!deviceManager_.device()) {
        lastParticleDebug_ =
            QStringLiteral("state=device-null skipped=device-null count=%1 path=particle")
                .arg(data.particles.size());
        qWarning() << "[ParticleRenderer] drawParticles skipped: device is null"
                   << "count=" << data.particles.size();
        return;
      }
      // Lazy initialization of particle renderer
      if (!gpuContext_) {
        gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(deviceManager_.device(), deviceManager_.immediateContext());
      }
      particleRenderer_ = std::make_unique<ArtifactCore::ParticleRenderer>(*gpuContext_);
      particleRenderer_->setFrameCostStats(nullptr);
      particleRenderer_->initialize(100000); // Support up to 100k particles
      submitter_.setParticleRenderer(particleRenderer_.get());
      qDebug() << "[ParticleRenderer] Initialized (max 100k particles)";
    }

    if (m_viewportWidth <= 0.0f || m_viewportHeight <= 0.0f) {
      lastParticleDebug_ = QStringLiteral(
                               "state=invalid-viewport skipped=invalid-viewport count=%1 viewport=%2x%3 path=particle")
                               .arg(data.particles.size())
                               .arg(m_viewportWidth)
                               .arg(m_viewportHeight);
      qWarning() << "[ParticleRenderer] drawParticles skipped: invalid viewport"
                 << "count=" << data.particles.size()
                 << "viewport=(" << m_viewportWidth << "x" << m_viewportHeight << ")";
      return;
    }

    // Particle VS uses dot(localPos, ViewRow) element-wise, so Qt matrices
    // are uploaded directly (no transpose needed).
    float panX = 0.0f;
    float panY = 0.0f;
    primitiveRenderer_.getPan(panX, panY);
    const float zoom = primitiveRenderer_.getZoom();

    qDebug() << "[ParticleRenderer] Drawing" << data.particles.size()
             << "particles camera3D=" << particle3DCameraActive_
             << "zoom=" << zoom << "pan=(" << panX << "," << panY << ")"
             << "viewport=(" << m_viewportWidth << "x" << m_viewportHeight << ")";

    QMatrix4x4 view;
    QMatrix4x4 proj;
    if (particle3DCameraActive_) {
      view = particleViewMatrix_;
      proj = particleProjMatrix_;
    } else {
      // 2D fallback: mirror PrimitiveRenderer2D's canvas->NDC path exactly.
      view.setToIdentity();
      proj.setToIdentity();
      proj.translate(-1.0f, 1.0f, 0.0f);
      proj.scale(2.0f / m_viewportWidth, -2.0f / m_viewportHeight, 1.0f);
      const bool disablePanZoom =
          qEnvironmentVariableIsSet("ARTIFACT_DEBUG_PARTICLE_NO_PAN_ZOOM");
      if (!disablePanZoom) {
        proj.scale(zoom, zoom, 1.0f);
        proj.translate(panX / std::max(zoom, 0.001f),
                       panY / std::max(zoom, 0.001f),
                       0.0f);
      }
    }

    qInfo() << "[ParticleRenderer] matrices"
            << "viewRow0=" << view.row(0)
            << "viewRow1=" << view.row(1)
            << "viewRow2=" << view.row(2)
            << "viewRow3=" << view.row(3)
            << "projRow0=" << proj.row(0)
            << "projRow1=" << proj.row(1)
            << "projRow2=" << proj.row(2)
            << "projRow3=" << proj.row(3)
            << "panZoomDisabled="
            << (qEnvironmentVariableIsSet("ARTIFACT_DEBUG_PARTICLE_NO_PAN_ZOOM") ? 1 : 0);

    auto* pRTV = primitiveRenderer_.currentRTV();
    if (!pRTV) {
      lastParticleDebug_ = QStringLiteral(
                               "state=no-rtv skipped=no-rtv count=%1 camera3D=%2 viewport=%3x%4 path=particle")
                               .arg(data.particles.size())
                               .arg(particle3DCameraActive_ ? QStringLiteral("true")
                                                            : QStringLiteral("false"))
                               .arg(m_viewportWidth)
                               .arg(m_viewportHeight);
      qWarning() << "[ParticleRenderer] No active RTV — skipping particle draw"
                 << "count=" << data.particles.size()
                 << "camera3D=" << particle3DCameraActive_
                 << "viewport=(" << m_viewportWidth << "x" << m_viewportHeight << ")";
      return;
    }

    lastParticleDebug_ = QStringLiteral(
                             "state=queued count=%1 camera3D=%2 zoom=%3 pan=%4,%5 viewport=%6x%7 rtv=bound matrix=%8 path=particle")
                             .arg(data.particles.size())
                             .arg(particle3DCameraActive_ ? QStringLiteral("true")
                                                          : QStringLiteral("false"))
                             .arg(QString::number(zoom, 'f', 3))
                             .arg(QString::number(panX, 'f', 1))
                             .arg(QString::number(panY, 'f', 1))
                             .arg(m_viewportWidth)
                             .arg(m_viewportHeight)
                             .arg(particle3DCameraActive_ ? QStringLiteral("3d")
                                                          : QStringLiteral("2d"));
    cmdBuf_.targetRTV = pRTV;
    ParticlePkt pkt;
    pkt.data = data;
    pkt.viewMatrix = view;
    pkt.projMatrix = proj;
    cmdBuf_.append(std::move(pkt));

    QImage debugBillboard(128, 128, QImage::Format_ARGB32_Premultiplied);
    debugBillboard.fill(QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f));
    {
      QPainter painter(&debugBillboard);
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setPen(QPen(QColor::fromRgbF(1.0f, 0.34f, 0.12f, 1.0f), 10.0));
      painter.setBrush(QColor::fromRgbF(0.98f, 0.76f, 0.18f, 0.95f));
      painter.drawRoundedRect(QRectF(10.0, 10.0, 108.0, 108.0), 22.0, 22.0);
      painter.setPen(QPen(QColor::fromRgbF(0.10f, 0.10f, 0.12f, 1.0f), 7.0));
      painter.drawLine(QPointF(28.0, 64.0), QPointF(100.0, 64.0));
      painter.drawLine(QPointF(64.0, 28.0), QPointF(64.0, 100.0));
    }
    primitiveRenderer3D_.drawBillboardQuad(
        QVector3D(m_viewportWidth * 0.5f, m_viewportHeight * 0.5f, 0.0f),
        QVector2D(320.0f, 320.0f), debugBillboard,
        FloatColor{1.0f, 1.0f, 1.0f, 1.0f}, 1.0f, 0.0f);
    qInfo() << "[ParticleRenderer] billboard-test drawn"
            << "center=" << m_viewportWidth * 0.5f << m_viewportHeight * 0.5f
            << "size=320x320";
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
   ScopedStartupTimer totalTimer("ArtifactIRenderer::initialize",
                                  StartupPhase::TotalStartup);
   widget_ = widget;
   meshRenderers_.clear();
   meshRendererGeometry_.clear();
   gpuContext_.reset();

   {
    ScopedStartupTimer t("DeviceManager::initialize", StartupPhase::DeviceCreation);
    deviceManager_.initialize(widget);
   }

   if (!deviceManager_.isInitialized()) {
    qWarning() << "[ArtifactIRenderer] initialize() failed: deviceManager not initialized"
               << "widget=" << widget << "size=" << (widget ? widget->size() : QSize());
    return;
   }

  shaderManager_.initialize(deviceManager_.device(), RenderConfig::MainRTVFormat);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();

  {
   ScopedStartupTimer t("RayTracingManager::initialize", StartupPhase::RayTracingInit);
   rayTracingManager_ = ArtifactCore::createRayTracingManager();
   if (rayTracingManager_->initialize(deviceManager_.device()) &&
       rayTracingManager_->isSupported()) {
    const bool builtTLAS = rayTracingManager_->buildTLAS(deviceManager_.immediateContext());
    const bool prepared = rayTracingManager_->ensurePipelineAndSBT(deviceManager_.immediateContext());
    const bool traced = rayTracingManager_->traceUnitQuad(deviceManager_.immediateContext(), 1, 1);
    qDebug() << "[ArtifactIRenderer] RayTracing init"
             << "supported=" << rayTracingManager_->isSupported()
             << "tlas=" << builtTLAS
             << "pipeline=" << prepared
             << "traceWarmup=" << traced;
   } else if (rayTracingManager_) {
    qDebug() << "[ArtifactIRenderer] RayTracing init skipped"
             << "supported=" << rayTracingManager_->isSupported();
   }
  }

  {
   ScopedStartupTimer t("PrimitiveRenderer2D::init", StartupPhase::Custom);
   primitiveRenderer_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
   primitiveRenderer_.setPSOs(shaderManager_);
   primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                 deviceManager_.swapChain());
   submitter_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
   submitter_.setPSOs(shaderManager_);
   submitter_.setFrameCostStats(nullptr);
   submitter_.setDeferredContext(deviceManager_.deferredContext());
   submitter_.setPrimitiveRenderer3D(&primitiveRenderer3D_);
   primitiveRenderer_.setCommandBuffer(&cmdBuf_);
  }

  gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(deviceManager_.device(),
                                                           deviceManager_.immediateContext());
  meshViewMatrix_.setToIdentity();
  meshProjMatrix_.setToIdentity();

  {
    ScopedStartupTimer t("PrimitiveRenderer3D::init", StartupPhase::Custom);
    primitiveRenderer3D_.createBuffers(deviceManager_.device());
    primitiveRenderer3D_.setPSOs(shaderManager_);
    primitiveRenderer3D_.setCommandBuffer(&cmdBuf_);
    primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
                                    deviceManager_.swapChain());
  }

  m_initialized = true;

  // Log startup profile summary
  qInfo().noquote() << QString::fromStdString(
      StartupProfiler::instance().generateReport());
 }

 void ArtifactIRenderer::Impl::initializeHeadless(int width, int height)
 {
  m_offlineWidth  = width;
  m_offlineHeight = height;
  meshRenderers_.clear();
  meshRendererGeometry_.clear();
  gpuContext_.reset();

  deviceManager_.initializeHeadless();
  if (!deviceManager_.isInitialized()) return;

  shaderManager_.initialize(deviceManager_.device(), RenderConfig::MainRTVFormat);
  shaderManager_.createShaders();
  shaderManager_.createPSOs();

  rayTracingManager_ = ArtifactCore::createRayTracingManager();
  if (rayTracingManager_->initialize(deviceManager_.device()) &&
      rayTracingManager_->isSupported()) {
   const bool builtTLAS = rayTracingManager_->buildTLAS(deviceManager_.immediateContext());
   const bool prepared = rayTracingManager_->ensurePipelineAndSBT(deviceManager_.immediateContext());
   const bool traced = rayTracingManager_->traceUnitQuad(deviceManager_.immediateContext(), 1, 1);
   qDebug() << "[ArtifactIRenderer] RayTracing init(headless)"
            << "supported=" << rayTracingManager_->isSupported()
            << "tlas=" << builtTLAS
            << "pipeline=" << prepared
            << "traceWarmup=" << traced;
  } else if (rayTracingManager_) {
   qDebug() << "[ArtifactIRenderer] RayTracing init(headless) skipped"
            << "supported=" << rayTracingManager_->isSupported();
  }

  primitiveRenderer_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  primitiveRenderer_.setPSOs(shaderManager_);
  submitter_.createBuffers(deviceManager_.device(), RenderConfig::MainRTVFormat);
  submitter_.setPSOs(shaderManager_);
  submitter_.setFrameCostStats(nullptr);
  submitter_.setDeferredContext(deviceManager_.deferredContext());
  submitter_.setPrimitiveRenderer3D(&primitiveRenderer3D_);
  primitiveRenderer_.setCommandBuffer(&cmdBuf_);

  gpuContext_ = std::make_unique<ArtifactCore::GpuContext>(deviceManager_.device(),
                                                           deviceManager_.immediateContext());
  meshViewMatrix_.setToIdentity();
  meshProjMatrix_.setToIdentity();

  primitiveRenderer3D_.createBuffers(deviceManager_.device());
  primitiveRenderer3D_.setPSOs(shaderManager_);
  primitiveRenderer3D_.setCommandBuffer(&cmdBuf_);

  TextureDesc TexDesc;
  TexDesc.Name      = "OfflineRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = static_cast<Uint32>(width);
  TexDesc.Height    = static_cast<Uint32>(height);
  TexDesc.MipLevels = 1;
  TexDesc.Format    = RenderConfig::MainRTVFormat;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);

  TextureDesc depthDesc;
  depthDesc.Name      = "OfflineDepthTarget";
  depthDesc.Type      = RESOURCE_DIM_TEX_2D;
  depthDesc.Width     = static_cast<Uint32>(width);
  depthDesc.Height    = static_cast<Uint32>(height);
  depthDesc.MipLevels = 1;
  depthDesc.Format    = TEX_FORMAT_D32_FLOAT;
  depthDesc.BindFlags = BIND_DEPTH_STENCIL;
  deviceManager_.device()->CreateTexture(depthDesc, nullptr, &m_layerDepthTex);

  auto* rtv = m_layerRT ? m_layerRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) : nullptr;
  primitiveRenderer_.setOverrideRTV(rtv);
  primitiveRenderer3D_.setOverrideRTV(rtv);
  primitiveRenderer3D_.setOverrideDSV(m_layerDepthTex ? m_layerDepthTex->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL) : nullptr);
  primitiveRenderer_.setContext(deviceManager_.immediateContext(), nullptr);
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext());
  primitiveRenderer_.setViewportSize(float(width), float(height));
  m_viewportWidth  = float(width);
  m_viewportHeight = float(height);
  setCanvasSize(float(width), float(height));
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
  return readbackTextureViewToImage(nullptr);
 }

 QImage ArtifactIRenderer::Impl::readbackTextureViewToImage(
     ITextureView* textureView) const
 {
  std::lock_guard<std::mutex> guard(m_readbackMutex);

  auto ctx    = deviceManager_.immediateContext();
  auto device = deviceManager_.device();
  if (!ctx || !device) return {};

  ITexture* srcTex  = nullptr;
  Uint32 srcWidth   = 0;
  Uint32 srcHeight  = 0;
  if (!resolveReadbackSourceTexture(textureView, m_layerRT,
                                    m_offlineWidth, m_offlineHeight,
                                    deviceManager_.swapChain(),
                                    srcTex, srcWidth, srcHeight)) {
    return {};
  }

  const TEXTURE_FORMAT srcFormat = srcTex->GetDesc().Format;
  const bool useFloatReadback = (srcFormat == TEX_FORMAT_RGBA16_FLOAT);
  const TEXTURE_FORMAT stagingFormat =
      useFloatReadback ? TEX_FORMAT_RGBA16_FLOAT : TEX_FORMAT_RGBA8_UNORM;

  // Staging textures mirror the source format so we can either memcpy raw bytes
  // for 8-bit sources or unpack half-floats for the HDR swap chain path. A 2-slot
  // ring is (re)allocated as a unit whenever the dimensions/format change.
  const bool ringNeedsRealloc =
      (m_readbackStagingWidth  != srcWidth) ||
      (m_readbackStagingHeight != srcHeight) ||
      (m_readbackStagingFormat != stagingFormat);
  if (ringNeedsRealloc) {
    for (auto& slot : m_readbackRing) {
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
      device->CreateTexture(stagDesc, nullptr, &slot.staging);
      if (!slot.staging) return {};

      FenceDesc fDesc;
      fDesc.Name = "ReadbackFence";
      fDesc.Type = FENCE_TYPE_GENERAL;
      device->CreateFence(fDesc, &slot.fence);
      if (!slot.fence) return {};
      slot.signaledValue  = 0;
      slot.completedValue = 0;
    }
    m_readbackStagingWidth  = srcWidth;
    m_readbackStagingHeight = srcHeight;
    m_readbackStagingFormat = stagingFormat;
    m_readbackRingIndex = 0;
  }

  // Advance to the next ring slot. If the *other* slot still has a copy in
  // flight, wait for it before we overwrite it — but in the steady state of
  // alternating captures this wait is already satisfied, so it adds no latency.
  ReadbackSlot& slot = m_readbackRing[m_readbackRingIndex];
  m_readbackRingIndex = (m_readbackRingIndex + 1) % kReadbackRingSize;
  if (slot.signaledValue > slot.completedValue) {
    slot.fence->Wait(slot.signaledValue);
    slot.completedValue = slot.signaledValue;
  }

  // Flush queued draws before reading back so both the 2D command buffer and
  // the 3D gizmo batch are materialized in the source texture.
  submitQueuedDraws(ctx);

  // Transition both textures to the required states and issue the copy.
  // Unbind the render target first so Vulkan doesn't complain about copying
  // from a texture that is still attached as an RTV.
  ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture              = srcTex;
  copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copyAttribs.pDstTexture              = slot.staging;
  copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  ctx->CopyTexture(copyAttribs);

  // Per-slot fence: CPU waits until this slot's GPU copy completes before mapping.
  const Uint64 waitValue = ++slot.signaledValue;
  ctx->EnqueueSignal(slot.fence, waitValue);
  ctx->Flush();
  slot.fence->Wait(waitValue);
  slot.completedValue = waitValue;

  // Map the staging texture. The fence wait above guarantees the GPU copy has
  // finished, so DO_NOT_WAIT is safe and avoids Vulkan backend warnings.
  MappedTextureSubresource mapped = {};
  ctx->MapTextureSubresource(slot.staging, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
  if (!mapped.pData) return {};

  QImage result(static_cast<int>(srcWidth), static_cast<int>(srcHeight),
                QImage::Format_RGBA8888);
  const size_t copyRowBytes = static_cast<size_t>(srcWidth) * 4u;
  const size_t sourceRowBytes =
      useFloatReadback ? static_cast<size_t>(srcWidth) * 8u : copyRowBytes;
  if (mapped.Stride < sourceRowBytes) {
   ctx->UnmapTextureSubresource(slot.staging, 0, 0);
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
     dst[x * 4 + 0] = linearToSrgb8(r);
     dst[x * 4 + 1] = linearToSrgb8(g);
     dst[x * 4 + 2] = linearToSrgb8(b);
     dst[x * 4 + 3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
    }
    srcRow = reinterpret_cast<const uint16_t*>(
        reinterpret_cast<const uint8_t*>(srcRow) + mapped.Stride);
   }
  }
  ctx->UnmapTextureSubresource(slot.staging, 0, 0);
  return result;
 }

 QImage ArtifactIRenderer::Impl::readbackDepthToImage() const
 {
  std::lock_guard<std::mutex> guard(m_readbackMutex);

  auto ctx = deviceManager_.immediateContext();
  auto device = deviceManager_.device();
  if (!ctx || !device) return {};

  ITexture* srcTex = nullptr;
  Uint32 srcWidth = 0;
  Uint32 srcHeight = 0;

  if (m_overrideDepthDSV && m_overrideDepthDSV->GetTexture()) {
   srcTex = m_overrideDepthDSV->GetTexture();
   const auto& desc = srcTex->GetDesc();
   srcWidth = desc.Width;
   srcHeight = desc.Height;
  } else if (m_layerDepthTex && m_offlineWidth > 0 && m_offlineHeight > 0) {
   srcTex = m_layerDepthTex;
   srcWidth = static_cast<Uint32>(m_offlineWidth);
   srcHeight = static_cast<Uint32>(m_offlineHeight);
  } else if (m_layerDepthTex) {
   const auto& desc = m_layerDepthTex->GetDesc();
   srcTex = m_layerDepthTex;
   srcWidth = desc.Width;
   srcHeight = desc.Height;
  } else if (auto sc = deviceManager_.swapChain()) {
   if (auto* dsv = sc->GetDepthBufferDSV()) {
    srcTex = dsv->GetTexture();
    if (srcTex) {
     const auto& desc = srcTex->GetDesc();
     srcWidth = desc.Width;
     srcHeight = desc.Height;
    }
   }
  }

  if (!srcTex || srcWidth == 0 || srcHeight == 0) return {};

  // Depth staging/fence are cached: depth inspection happens repeatedly (gizmo
  // picking, debug overlays) and recreating them per call generated avoidable
  // device traffic. Reallocate only when the source size changes.
  if (!m_depthReadbackStaging ||
      m_depthReadbackWidth != srcWidth ||
      m_depthReadbackHeight != srcHeight) {
    TextureDesc stagDesc;
    stagDesc.Name           = "DepthReadbackStagingTexture";
    stagDesc.Type           = RESOURCE_DIM_TEX_2D;
    stagDesc.Width          = srcWidth;
    stagDesc.Height         = srcHeight;
    stagDesc.MipLevels      = 1;
    stagDesc.Format         = TEX_FORMAT_D32_FLOAT;
    stagDesc.Usage          = USAGE_STAGING;
    stagDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagDesc.BindFlags      = BIND_NONE;
    device->CreateTexture(stagDesc, nullptr, &m_depthReadbackStaging);
    if (!m_depthReadbackStaging) return {};
    m_depthReadbackWidth  = srcWidth;
    m_depthReadbackHeight = srcHeight;
  }
  ITexture* stagingTex = m_depthReadbackStaging;

  if (!m_depthReadbackFence) {
    FenceDesc fenceDesc;
    fenceDesc.Name = "DepthReadbackFence";
    fenceDesc.Type = FENCE_TYPE_GENERAL;
    device->CreateFence(fenceDesc, &m_depthReadbackFence);
    if (!m_depthReadbackFence) return {};
    m_depthReadbackFenceValue = 0;
  }
  IFence* fence = m_depthReadbackFence;

  submitQueuedDraws(ctx);
  ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture = srcTex;
  copyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  copyAttribs.pDstTexture = stagingTex;
  copyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
  ctx->CopyTexture(copyAttribs);
  const Uint64 fenceValue = ++m_depthReadbackFenceValue;
  ctx->EnqueueSignal(fence, fenceValue);
  ctx->Flush();
  fence->Wait(fenceValue);

  MappedTextureSubresource mapped = {};
  ctx->MapTextureSubresource(stagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
  if (!mapped.pData) return {};

  QImage result(static_cast<int>(srcWidth), static_cast<int>(srcHeight), QImage::Format_Grayscale8);
  if (mapped.Stride < static_cast<size_t>(srcWidth) * sizeof(float)) {
   ctx->UnmapTextureSubresource(stagingTex, 0, 0);
   return {};
  }

  const auto* srcRow = static_cast<const float*>(mapped.pData);
  for (Uint32 row = 0; row < srcHeight; ++row) {
   auto* dst = result.scanLine(static_cast<int>(row));
   const auto* rowPtr = srcRow;
   for (Uint32 x = 0; x < srcWidth; ++x) {
    const float depth = std::clamp(rowPtr[x], 0.0f, 1.0f);
    dst[x] = static_cast<uint8_t>((1.0f - depth) * 255.0f + 0.5f);
   }
   srcRow = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(srcRow) + mapped.Stride);
  }

  ctx->UnmapTextureSubresource(stagingTex, 0, 0);
  return result;
 }

 QImage ArtifactIRenderer::Impl::readbackChannelToImage(ArtifactIRenderer::ChannelType channel) const
 {
  if (channel == ArtifactIRenderer::ChannelType::Depth) {
   return readbackDepthToImage();
  }

  const int offset = rgbaOffsetForChannel(channel);
  if (offset < 0) {
   return {};
  }

  const QImage color = readbackToImage();
  if (color.isNull()) {
   return {};
  }

  const QImage rgba = (color.format() == QImage::Format_RGBA8888)
                          ? color
                          : color.convertToFormat(QImage::Format_RGBA8888);
  QImage channelImage(rgba.width(), rgba.height(), QImage::Format_Grayscale8);
  for (int y = 0; y < rgba.height(); ++y) {
   const auto* src = rgba.constScanLine(y);
   auto* dst = channelImage.scanLine(y);
   for (int x = 0; x < rgba.width(); ++x) {
    dst[x] = src[x * 4 + offset];
   }
  }
  return channelImage;
 }

 void ArtifactIRenderer::Impl::readbackToImageAsync(ReadbackCallback callback) const
 {
  readbackTextureViewToImageAsync(nullptr, std::move(callback));
 }

 void ArtifactIRenderer::Impl::readbackTextureViewToImageAsync(
     ITextureView* textureView,
     ReadbackCallback callback) const
 {
  if (!deviceManager_.device() || !deviceManager_.immediateContext()) {
    if (callback) callback(QImage());
    return;
  }

  auto ctx = deviceManager_.immediateContext();
  auto device = deviceManager_.device();

  ITexture* srcTexPtr = nullptr;
  Uint32 srcWidth = 0, srcHeight = 0;
  if (!resolveReadbackSourceTexture(textureView, m_layerRT,
                                    m_offlineWidth, m_offlineHeight,
                                    deviceManager_.swapChain(),
                                    srcTexPtr, srcWidth, srcHeight)) {
    if (callback) callback(QImage());
    return;
  }

  const TEXTURE_FORMAT srcFormat = srcTexPtr->GetDesc().Format;
  const bool useFloatReadback = (srcFormat == TEX_FORMAT_RGBA16_FLOAT);
  const TEXTURE_FORMAT stagingFormat =
      useFloatReadback ? TEX_FORMAT_RGBA16_FLOAT : TEX_FORMAT_RGBA8_UNORM;

  RefCntAutoPtr<ITexture> stagingTex;
  RefCntAutoPtr<IFence> fence;
  std::shared_ptr<std::atomic_bool> busyFlag;
  bool cachedSlot = false;

  auto createAsyncResources = [&](RefCntAutoPtr<ITexture>& outStaging,
                                  RefCntAutoPtr<IFence>& outFence,
                                  const char* name) -> bool {
    TextureDesc stagDesc;
    stagDesc.Name           = name;
    stagDesc.Type           = RESOURCE_DIM_TEX_2D;
    stagDesc.Width          = srcWidth;
    stagDesc.Height         = srcHeight;
    stagDesc.MipLevels      = 1;
    stagDesc.Format         = stagingFormat;
    stagDesc.Usage          = USAGE_STAGING;
    stagDesc.CPUAccessFlags = CPU_ACCESS_READ;
    stagDesc.BindFlags      = BIND_NONE;
    device->CreateTexture(stagDesc, nullptr, &outStaging);
    if (!outStaging) {
      return false;
    }
    FenceDesc fDesc;
    fDesc.Name = name;
    fDesc.Type = FENCE_TYPE_GENERAL;
    device->CreateFence(fDesc, &outFence);
    return outFence != nullptr;
  };

  {
    std::lock_guard<std::mutex> guard(m_readbackMutex);
    const bool formatChanged =
        m_asyncReadbackStagingWidth != srcWidth ||
        m_asyncReadbackStagingHeight != srcHeight ||
        m_asyncReadbackStagingFormat != stagingFormat;
    if (formatChanged) {
      bool anyBusy = false;
      for (const auto& slot : m_asyncReadbackRing) {
        anyBusy = anyBusy || (slot.busy && slot.busy->load());
      }
      if (!anyBusy) {
        for (auto& slot : m_asyncReadbackRing) {
          slot.staging = nullptr;
          slot.fence = nullptr;
          slot.width = 0;
          slot.height = 0;
          slot.format = TEX_FORMAT_UNKNOWN;
          if (!slot.busy) {
            slot.busy = std::make_shared<std::atomic_bool>(false);
          }
          slot.busy->store(false);
        }
        m_asyncReadbackStagingWidth = srcWidth;
        m_asyncReadbackStagingHeight = srcHeight;
        m_asyncReadbackStagingFormat = stagingFormat;
        m_asyncReadbackRingIndex = 0;
      }
    }

    for (Uint32 attempt = 0; attempt < kAsyncReadbackRingSize; ++attempt) {
      const Uint32 index = (m_asyncReadbackRingIndex + attempt) % kAsyncReadbackRingSize;
      auto& slot = m_asyncReadbackRing[index];
      if (slot.busy && slot.busy->load()) {
        continue;
      }
      if (!slot.busy) {
        slot.busy = std::make_shared<std::atomic_bool>(false);
      }
      if (!slot.staging || !slot.fence ||
          slot.width != srcWidth ||
          slot.height != srcHeight ||
          slot.format != stagingFormat) {
        if (!createAsyncResources(slot.staging, slot.fence, "AsyncReadbackRing")) {
          slot.staging = nullptr;
          slot.fence = nullptr;
          slot.width = 0;
          slot.height = 0;
          slot.format = TEX_FORMAT_UNKNOWN;
          continue;
        }
        slot.width = srcWidth;
        slot.height = srcHeight;
        slot.format = stagingFormat;
      }
      slot.busy->store(true);
      stagingTex = slot.staging;
      fence = slot.fence;
      busyFlag = slot.busy;
      cachedSlot = true;
      m_asyncReadbackRingIndex = (index + 1) % kAsyncReadbackRingSize;
      m_asyncReadbackStagingWidth = srcWidth;
      m_asyncReadbackStagingHeight = srcHeight;
      m_asyncReadbackStagingFormat = stagingFormat;
      break;
    }
  }

  if (!stagingTex || !fence) {
    if (!createAsyncResources(stagingTex, fence, "AsyncReadbackOneShot")) {
      if (callback) callback(QImage());
      return;
    }
  }

  // Unbind render target, then copy
  ctx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

  CopyTextureAttribs copyAttribs;
  copyAttribs.pSrcTexture              = srcTexPtr;
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
    [[maybe_unused]] auto future = QtConcurrent::run([fence, stagingTex, ctx, w, h, floatReadback,
                                                       waitValue, busyFlag, cachedSlot,
                                                       cb = std::move(callback)]() mutable {
    auto releaseSlot = [&]() {
      if (cachedSlot && busyFlag) {
        busyFlag->store(false);
      }
    };
    // Wait for GPU copy to complete
    fence->Wait(waitValue);

    // Map staging texture
    MappedTextureSubresource mapped = {};
    ctx->MapTextureSubresource(stagingTex, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, mapped);
    if (!mapped.pData) {
      releaseSlot();
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
      releaseSlot();
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
          dst[x * 4 + 0] = linearToSrgb8(r);
          dst[x * 4 + 1] = linearToSrgb8(g);
          dst[x * 4 + 2] = linearToSrgb8(b);
          dst[x * 4 + 3] = static_cast<uint8_t>(a * 255.0f + 0.5f);
        }
        srcRow = reinterpret_cast<const uint16_t*>(
            reinterpret_cast<const uint8_t*>(srcRow) + mapped.Stride);
      }
    }

    ctx->UnmapTextureSubresource(stagingTex, 0, 0);
    releaseSlot();

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
  const float upscaleScale = std::clamp(upscaleRenderScale(), 0.25f, 1.0f);
  const Uint32 targetWidth =
      std::max<Uint32>(1, static_cast<Uint32>(std::round(static_cast<float>(newWidth) * upscaleScale)));
  const Uint32 targetHeight =
      std::max<Uint32>(1, static_cast<Uint32>(std::round(static_cast<float>(newHeight) * upscaleScale)));

  if (m_layerRT && m_layerRTWidth == targetWidth && m_layerRTHeight == targetHeight) {
    return;
  }

  if (m_layerRT) m_layerRT.Release();
  if (m_layerDepthTex) m_layerDepthTex.Release();

  TextureDesc TexDesc;
  TexDesc.Name      = "LayerRenderTarget";
  TexDesc.Type      = RESOURCE_DIM_TEX_2D;
  TexDesc.Width     = targetWidth;
  TexDesc.Height    = targetHeight;
  TexDesc.MipLevels = 1;
  TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
  deviceManager_.device()->CreateTexture(TexDesc, nullptr, &m_layerRT);

  TextureDesc depthDesc;
  depthDesc.Name      = "LayerDepthTarget";
  depthDesc.Type      = RESOURCE_DIM_TEX_2D;
  depthDesc.Width     = targetWidth;
  depthDesc.Height    = targetHeight;
  depthDesc.MipLevels = 1;
  depthDesc.Format    = TEX_FORMAT_D32_FLOAT;
  depthDesc.BindFlags = BIND_DEPTH_STENCIL;
  deviceManager_.device()->CreateTexture(depthDesc, nullptr, &m_layerDepthTex);
  m_layerRTWidth = targetWidth;
  m_layerRTHeight = targetHeight;
 }

Diligent::ITextureView* ArtifactIRenderer::Impl::activeDepthView() const
{
  if (m_overrideDepthDSV) {
   return m_overrideDepthDSV;
  }
  if (m_layerDepthTex) {
   return m_layerDepthTex->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
  }
  if (auto sc = deviceManager_.swapChain()) {
   return sc->GetDepthBufferDSV();
  }
  return nullptr;
}

 Diligent::ITextureView* ArtifactIRenderer::Impl::activeColorView() const
 {
  if (m_overrideColorRTV) {
   return m_overrideColorRTV;
  }
  if (m_layerRT) {
   return m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  }
  if (auto sc = deviceManager_.swapChain()) {
   return sc->GetCurrentBackBufferRTV();
  }
  return nullptr;
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
   submitter_.setFrameCostStats(nullptr);
   submitter_.setDeferredContext(deviceManager_.deferredContext());
   primitiveRenderer_.setCommandBuffer(&cmdBuf_);
   m_initialized = true;
  }

  primitiveRenderer_.setContext(deviceManager_.immediateContext(),
                                 deviceManager_.swapChain());
  primitiveRenderer3D_.setContext(deviceManager_.immediateContext(),
                                  deviceManager_.swapChain());
  primitiveRenderer3D_.setOverrideDSV(nullptr);
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
  primitiveRenderer3D_.setOverrideDSV(nullptr);
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
  if (!qEnvironmentVariableIsSet("ARTIFACT_ENABLE_GPU_FRAME_QUERY")) {
   return;
  }
  initFrameQueries();
  auto& query = m_frameQueries[m_frameQueryIndex];
  if (!query || !deviceManager_.immediateContext()) return;
  deviceManager_.immediateContext()->BeginQuery(query);
  m_frameQueryActive = true;
 }

 void ArtifactIRenderer::Impl::endFrameGpuProfiling()
 {
  if (!m_frameQueryActive) {
   return;
  }
  m_frameQueryActive = false;
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

void ArtifactIRenderer::Impl::beginFrameCostCapture()
{
  m_currentFrameCostStats_ = {};
  lastParticleDebug_.clear();
  submitter_.beginFrameDebugCapture();
  submitter_.setFrameCostStats(&m_currentFrameCostStats_);
  primitiveRenderer3D_.setFrameCostStats(&m_currentFrameCostStats_);
}

void ArtifactIRenderer::Impl::endFrameCostCapture()
{
  submitter_.setFrameCostStats(nullptr);
  primitiveRenderer3D_.setFrameCostStats(nullptr);
  submitter_.endFrameDebugCapture();
  m_lastFrameCostStats_ = m_currentFrameCostStats_;
}

ArtifactCore::RenderCostStats ArtifactIRenderer::Impl::frameCostStats() const
{
  return m_lastFrameCostStats_;
}

std::vector<ArtifactCore::FrameDebugPassRecord> ArtifactIRenderer::Impl::frameDebugPasses() const
{
  return submitter_.frameDebugPasses();
}

 // ---------------------------------------------------------------------------
 // clear / flush / destroy
 // ---------------------------------------------------------------------------

 void ArtifactIRenderer::Impl::clear()
 {
  primitiveRenderer_.clear(deviceManager_.immediateContext(), clearColor_);
  if (auto ctx = deviceManager_.immediateContext()) {
   if (auto* dsv = activeDepthView()) {
    ctx->SetRenderTargets(0, nullptr, dsv,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ctx->ClearDepthStencil(dsv, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
                           Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   }
  }
 }

 void ArtifactIRenderer::Impl::setClearColor(const FloatColor& color)
 {
  clearColor_ = color;
 }

 void ArtifactIRenderer::Impl::setMultiChannelEnabled(bool enabled)
 {
  m_multiChannelEnabled = enabled;
 }

 void ArtifactIRenderer::Impl::setChannelEnabled(ArtifactIRenderer::ChannelType channel, bool enabled)
 {
  const auto index = static_cast<size_t>(channel);
  if (index >= m_channelEnabled.size()) {
   return;
  }
  m_channelEnabled[index] = enabled;
 }

 bool ArtifactIRenderer::Impl::isChannelEnabled(ArtifactIRenderer::ChannelType channel) const
 {
  if (!m_multiChannelEnabled) {
   return isBaseRgbaChannel(channel);
  }
  const auto index = static_cast<size_t>(channel);
  if (index >= m_channelEnabled.size()) {
   return false;
  }
  return m_channelEnabled[index];
 }

 void ArtifactIRenderer::Impl::flush()
 {
  if (auto ctx = deviceManager_.immediateContext()) {
   submitQueuedDraws(ctx);
   ctx->Flush();
  }
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
  submitQueuedDraws(ctx);
  ctx->Flush();
  ctx->EnqueueSignal(fence, 1);
  ctx->Flush();
  fence->Wait(1);
 }

  void ArtifactIRenderer::Impl::destroy()
  {
  if (particleRenderer_) {
   particleRenderer_->setFrameCostStats(nullptr);
  }
  submitter_.setParticleRenderer(nullptr);
  submitter_.setPrimitiveRenderer3D(nullptr);
  submitter_.destroy();
  cmdBuf_.reset();
  meshRenderers_.clear();
  meshRendererGeometry_.clear();
  for (auto& slot : m_readbackRing) {
   slot.staging = nullptr;
   slot.fence   = nullptr;
   slot.signaledValue  = 0;
   slot.completedValue = 0;
  }
  for (auto& slot : m_asyncReadbackRing) {
   slot.staging = nullptr;
   slot.fence = nullptr;
   slot.width = 0;
   slot.height = 0;
   slot.format = TEX_FORMAT_UNKNOWN;
   if (slot.busy) {
    slot.busy->store(false);
   }
  }
  m_readbackRingIndex    = 0;
  m_asyncReadbackRingIndex = 0;
  m_readbackStagingWidth  = 0;
  m_readbackStagingHeight = 0;
  m_readbackStagingFormat = TEX_FORMAT_UNKNOWN;
  m_asyncReadbackStagingWidth = 0;
  m_asyncReadbackStagingHeight = 0;
  m_asyncReadbackStagingFormat = TEX_FORMAT_UNKNOWN;
  m_depthReadbackStaging    = nullptr;
  m_depthReadbackFence      = nullptr;
  m_depthReadbackWidth      = 0;
  m_depthReadbackHeight     = 0;
  m_depthReadbackFenceValue = 0;
  m_layerRT = nullptr;
  m_layerDepthTex = nullptr;
  for (auto& query : m_frameQueries) query = nullptr;
  m_frameQueryActive = false;
  primitiveRenderer_.destroy();
  primitiveRenderer3D_.destroy();
  particleRenderer_.reset();
  gpuContext_.reset();
  shaderManager_.destroy();
  reportLiveD3D12Objects(deviceManager_.device().RawPtr());
  deviceManager_.destroy();
  widget_                = nullptr;
  m_initialized          = false;
  m_frameQueryInitialized = false;
 }

  void ArtifactIRenderer::Impl::present()
   {
    ++presentAttemptCount_;
    if (auto sc = deviceManager_.swapChain())
    {
     submitQueuedDraws(deviceManager_.immediateContext());
      try {
       sc->Present();
       ++presentSuccessCount_;
       lastPresentStatus_ = QStringLiteral("ok");
     } catch (const std::exception& ex) {
     ++presentFailureCount_;
     const QString msg = QString::fromLocal8Bit(ex.what());
     lastPresentStatus_ = msg;
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
    ++presentSkippedCount_;
    lastPresentStatus_ = QStringLiteral("skipped-no-swapchain");
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
 quint64 ArtifactIRenderer::presentAttemptCount() const { return impl_->presentAttemptCount(); }
 quint64 ArtifactIRenderer::presentSuccessCount() const { return impl_->presentSuccessCount(); }
 quint64 ArtifactIRenderer::presentFailureCount() const { return impl_->presentFailureCount(); }
 quint64 ArtifactIRenderer::presentSkippedCount() const { return impl_->presentSkippedCount(); }
 QString ArtifactIRenderer::lastPresentStatus() const { return impl_->lastPresentStatus(); }
 void ArtifactIRenderer::beginFrameCostCapture() { impl_->beginFrameCostCapture(); }
 void ArtifactIRenderer::endFrameCostCapture() { impl_->endFrameCostCapture(); }
 void ArtifactIRenderer::beginFrameGpuProfiling() { impl_->beginFrameGpuProfiling(); }
 void ArtifactIRenderer::endFrameGpuProfiling() { impl_->endFrameGpuProfiling(); }
 ArtifactCore::RenderCostStats ArtifactIRenderer::frameCostStats() const { return impl_->frameCostStats(); }
 std::vector<ArtifactCore::FrameDebugPassRecord> ArtifactIRenderer::frameDebugPasses() const { return impl_->frameDebugPasses(); }
 double ArtifactIRenderer::lastFrameGpuTimeMs() const { return impl_->lastFrameGpuTimeMs(); }

 QImage ArtifactIRenderer::readbackToImage() const { return impl_->readbackToImage(); }
 QImage ArtifactIRenderer::readbackTextureViewToImage(
     Diligent::ITextureView* textureView) const
 {
  return impl_->readbackTextureViewToImage(textureView);
 }
 QImage ArtifactIRenderer::readbackDepthToImage() const { return impl_->readbackDepthToImage(); }
 void ArtifactIRenderer::readbackTextureViewToImageAsync(
     Diligent::ITextureView* textureView,
     ReadbackCallback callback) const
 {
  impl_->readbackTextureViewToImageAsync(textureView, std::move(callback));
 }
 QImage ArtifactIRenderer::readbackChannelToImage(ChannelType channel) const
 {
  return impl_->readbackChannelToImage(channel);
 }
 ArtifactCore::MultiChannelImage ArtifactIRenderer::readbackToMultiChannelImage() const
 {
  const bool needRgba =
      impl_->isChannelEnabled(ChannelType::Red) ||
      impl_->isChannelEnabled(ChannelType::Green) ||
      impl_->isChannelEnabled(ChannelType::Blue) ||
      impl_->isChannelEnabled(ChannelType::Alpha);
  const bool needDepth = impl_->isChannelEnabled(ChannelType::Depth);

  QImage rgba;
  if (needRgba) {
   const QImage color = impl_->readbackToImage();
   if (!color.isNull()) {
    rgba = (color.format() == QImage::Format_RGBA8888)
               ? color
               : color.convertToFormat(QImage::Format_RGBA8888);
   }
  }

  QImage depth;
  if (needDepth) {
   depth = impl_->readbackDepthToImage();
  }

  QSize imageSize;
  if (!rgba.isNull()) {
   imageSize = rgba.size();
  } else if (!depth.isNull()) {
   imageSize = depth.size();
  } else {
   return {};
  }

  ArtifactCore::MultiChannelImage image(imageSize.width(), imageSize.height());
  constexpr std::array<ChannelType, 4> baseChannels = {
      ChannelType::Red, ChannelType::Green, ChannelType::Blue, ChannelType::Alpha};
  for (ChannelType baseChannel : baseChannels) {
   if (!impl_->isChannelEnabled(baseChannel)) {
    image.removeChannel(toCoreChannel(baseChannel));
   }
  }

  auto writeRgbaChannel = [&](ChannelType channelType, int offset) {
   if (!impl_->isChannelEnabled(channelType) || rgba.isNull() || rgba.size() != imageSize) {
    return;
   }
   const ArtifactCore::ChannelType type = toCoreChannel(channelType);
   auto outChannel = image.getChannel(type);
   if (!outChannel) {
    image.addChannel(type);
    outChannel = image.getChannel(type);
   }
   if (!outChannel) {
    return;
   }
   for (int y = 0; y < rgba.height(); ++y) {
    const auto* src = rgba.constScanLine(y);
    auto* dst = outChannel->data() + static_cast<size_t>(y) * static_cast<size_t>(rgba.width());
    for (int x = 0; x < rgba.width(); ++x) {
     dst[x] = static_cast<float>(src[x * 4 + offset]) / 255.0f;
    }
   }
  };

  writeRgbaChannel(ChannelType::Red, 0);
  writeRgbaChannel(ChannelType::Green, 1);
  writeRgbaChannel(ChannelType::Blue, 2);
  writeRgbaChannel(ChannelType::Alpha, 3);

  if (needDepth && !depth.isNull() && depth.size() == imageSize) {
   auto depthChannel = image.getChannel(ArtifactCore::ChannelType::Depth);
   if (!depthChannel) {
    image.addChannel(ArtifactCore::ChannelType::Depth);
    depthChannel = image.getChannel(ArtifactCore::ChannelType::Depth);
   }
   if (depthChannel) {
    for (int y = 0; y < depth.height(); ++y) {
     const auto* src = depth.constScanLine(y);
     auto* dst = depthChannel->data() + static_cast<size_t>(y) * static_cast<size_t>(depth.width());
     for (int x = 0; x < depth.width(); ++x) {
      dst[x] = 1.0f - (static_cast<float>(src[x]) / 255.0f);
     }
    }
   }
  }

  return image;
 }
 void ArtifactIRenderer::readbackToImageAsync(ReadbackCallback callback) const {
  impl_->readbackToImageAsync(std::move(callback));
 }

 void ArtifactIRenderer::present()
 {
  impl_->present();
 }

 void ArtifactIRenderer::setClearColor(const FloatColor& color) { impl_->setClearColor(color); }
 void ArtifactIRenderer::setMultiChannelEnabled(bool enabled) { impl_->setMultiChannelEnabled(enabled); }
 bool ArtifactIRenderer::isMultiChannelEnabled() const { return impl_->isMultiChannelEnabled(); }
 void ArtifactIRenderer::setChannelEnabled(ArtifactIRenderer::ChannelType channel, bool enabled) { impl_->setChannelEnabled(channel, enabled); }
 bool ArtifactIRenderer::isChannelEnabled(ArtifactIRenderer::ChannelType channel) const { return impl_->isChannelEnabled(channel); }
  FloatColor ArtifactIRenderer::getClearColor() const { return impl_->getClearColor(); }
  void ArtifactIRenderer::setViewportSize(float w, float h) { impl_->setViewportSize(w, h); }
  void ArtifactIRenderer::setViewportRect(float w, float h) { impl_->setViewportRect(w, h); }
  void ArtifactIRenderer::unbindColorTargetsForCompute() { impl_->unbindColorTargetsForCompute(); }
  void ArtifactIRenderer::setDevicePixelRatio(float dpr) { impl_->primitiveRenderer_.setDevicePixelRatio(dpr); }
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
void ArtifactIRenderer::setStereoCameraMatrices(const QMatrix4x4& leftView,
                                               const QMatrix4x4& rightView,
                                               const QMatrix4x4& proj)
{ impl_->setStereoCameraMatrices(leftView, rightView, proj); }
void ArtifactIRenderer::resetStereoCameraMatrices()
{ impl_->resetStereoCameraMatrices(); }
void ArtifactIRenderer::setSceneLights(const std::vector<ArtifactCore::Light>& lights)
{ impl_->m_sceneLights = lights; }
const std::vector<ArtifactCore::Light>& ArtifactIRenderer::getSceneLights() const
{ return impl_->m_sceneLights; }

 QMatrix4x4 ArtifactIRenderer::getViewMatrix() const { return impl_->primitiveRenderer_.getViewMatrix(); }
 QMatrix4x4 ArtifactIRenderer::getProjectionMatrix() const { return impl_->primitiveRenderer_.getProjectionMatrix(); }

 void ArtifactIRenderer::zoomAroundViewportPoint(Detail::float2 pos, float newZoom)
 { impl_->zoomAroundViewportPoint(pos, newZoom); }
 Detail::float2 ArtifactIRenderer::canvasToViewport(Detail::float2 pos) const
 { return impl_->canvasToViewport(pos); }
 Detail::float2 ArtifactIRenderer::viewportToCanvas(Detail::float2 pos) const
 { return impl_->viewportToCanvas(pos); }

 // LOD Implementation
 void ArtifactIRenderer::setDetailLevel(LODManager::DetailLevel lod)
 { impl_->setDetailLevel(lod); }
 LODManager::DetailLevel ArtifactIRenderer::detailLevel() const
 { return impl_->detailLevel(); }

 void ArtifactIRenderer::drawRectOutline(float x, float y, float w, float h, const FloatColor& color)
 { impl_->drawRectOutline(x, y, w, h, color); }
 void ArtifactIRenderer::drawRectOutline(Detail::float2 pos, Detail::float2 size, const FloatColor& color)
 { impl_->drawRectOutline(toDiligentFloat2(pos), toDiligentFloat2(size), color); }
 void ArtifactIRenderer::drawDashedRectOutline(float x, float y, float w, float h,
                                              const FloatColor& color, float thickness,
                                              float dashLength, float gapLength)
 { impl_->drawDashedRectOutline(x, y, w, h, color, thickness, dashLength, gapLength); }
 void ArtifactIRenderer::drawDashedRectOutline(Detail::float2 pos, Detail::float2 size,
                                              const FloatColor& color, float thickness,
                                              float dashLength, float gapLength)
 { impl_->drawDashedRectOutline(toDiligentFloat2(pos), toDiligentFloat2(size), color, thickness, dashLength, gapLength); }
 void ArtifactIRenderer::drawOverlayPanel(float x, float y, float w, float h,
                                         const FloatColor& fillColor,
                                         const FloatColor& outlineColor,
                                         float opacity)
 { impl_->drawOverlayPanel(x, y, w, h, fillColor, outlineColor, opacity); }
 void ArtifactIRenderer::drawOverlayPanel(Detail::float2 pos, Detail::float2 size,
                                         const FloatColor& fillColor,
                                         const FloatColor& outlineColor,
                                         float opacity)
 { impl_->drawOverlayPanel(toDiligentFloat2(pos), toDiligentFloat2(size), fillColor, outlineColor, opacity); }
 void ArtifactIRenderer::drawRoundedPanel(float x, float y, float w, float h, float radius,
                                          const FloatColor& fillColor,
                                          const FloatColor& outlineColor,
                                          float opacity,
                                          float outlineThickness)
 { impl_->drawRoundedPanel(x, y, w, h, radius, fillColor, outlineColor, opacity, outlineThickness); }
 void ArtifactIRenderer::drawRoundedPanel(Detail::float2 pos, Detail::float2 size, float radius,
                                          const FloatColor& fillColor,
                                          const FloatColor& outlineColor,
                                          float opacity,
                                          float outlineThickness)
 { impl_->drawRoundedPanel(toDiligentFloat2(pos), toDiligentFloat2(size), radius, fillColor, outlineColor, opacity, outlineThickness); }
 void ArtifactIRenderer::drawSolidLine(Detail::float2 start, Detail::float2 end,
                                       const FloatColor& color, float thickness)
 { impl_->drawSolidLine(start, end, color, thickness); }
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
 void ArtifactIRenderer::drawText(const QRectF& rect, const QString& text,
                                  const QFont& font, const FloatColor& color,
                                  Qt::Alignment alignment, float opacity,
                                  const FloatColor& outlineColor, float outlineThickness)
 { impl_->primitiveRenderer_.drawText(rect, text, font, color, alignment, opacity, outlineColor, outlineThickness); }
 void ArtifactIRenderer::drawTextTransformed(const QRectF& rect, const QString& text,
                                             const QFont& font, const FloatColor& color,
                                             const QMatrix4x4& transform,
                                             Qt::Alignment alignment, float opacity,
                                             const FloatColor& outlineColor, float outlineThickness)
 { impl_->primitiveRenderer_.drawTextTransformed(rect, text, font, color, transform, alignment, opacity, outlineColor, outlineThickness); }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QTransform& transform, const QImage& image, float opacity)
 {
  // Direct delegation to primitive renderer for transformed sprite drawing
  impl_->primitiveRenderer_.drawSpriteTransformed(x, y, w, h, transform, image, opacity);
 }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const QImage& image, float opacity)
 {
  impl_->primitiveRenderer_.drawSpriteTransformed(x, y, w, h, transform, image, opacity);
 }
 void ArtifactIRenderer::drawSpriteTransformed(float x, float y, float w, float h, const QMatrix4x4& transform, const ArtifactCore::ImageF32x4_RGBA& image, float opacity)
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
void ArtifactIRenderer::drawSolidPolygonLocal(const std::vector<Detail::float2>& points,
                                              const FloatColor& color)
{ impl_->drawSolidPolygonLocal(points, color); }
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
void ArtifactIRenderer::drawParticles(const ArtifactCore::ParticleRenderData& data) { impl_->drawParticles(data); }

QString ArtifactIRenderer::particleDebugState() const {
  if (!impl_) {
    return QStringLiteral("<no renderer>");
  }
  QString state = impl_->lastParticleDebug_.isEmpty()
                      ? QStringLiteral("<none>")
                      : impl_->lastParticleDebug_;
  if (impl_->particleRenderer_) {
    const QString rendererState = impl_->particleRenderer_->debugState();
    if (!rendererState.isEmpty()) {
      if (state == QStringLiteral("<none>")) {
        state = rendererState;
      } else {
        state.append(QStringLiteral(" | "));
        state.append(rendererState);
      }
    }
  }
  return state;
}

QString ArtifactIRenderer::glyphAtlasDebugState() const {
  if (!impl_) {
    return QStringLiteral("<no renderer>");
  }
  const QString state = impl_->primitiveRenderer_.glyphAtlasDebugState();
  return state.isEmpty() ? QStringLiteral("<none>") : state;
}

namespace {
QString rayTracingDeviceTypeName(Diligent::RENDER_DEVICE_TYPE type)
{
  switch (type) {
  case Diligent::RENDER_DEVICE_TYPE_D3D12:
    return QStringLiteral("D3D12");
  case Diligent::RENDER_DEVICE_TYPE_VULKAN:
    return QStringLiteral("Vulkan");
  case Diligent::RENDER_DEVICE_TYPE_D3D11:
    return QStringLiteral("D3D11");
  case Diligent::RENDER_DEVICE_TYPE_GL:
    return QStringLiteral("GL");
  case Diligent::RENDER_DEVICE_TYPE_GLES:
    return QStringLiteral("GLES");
  case Diligent::RENDER_DEVICE_TYPE_METAL:
    return QStringLiteral("Metal");
  case Diligent::RENDER_DEVICE_TYPE_WEBGPU:
    return QStringLiteral("WebGPU");
  default:
    return QStringLiteral("Undefined");
  }
}

QString rayTracingFeatureStateName(Diligent::DEVICE_FEATURE_STATE state)
{
  switch (state) {
  case Diligent::DEVICE_FEATURE_STATE_ENABLED:
    return QStringLiteral("enabled");
  case Diligent::DEVICE_FEATURE_STATE_OPTIONAL:
    return QStringLiteral("optional");
  case Diligent::DEVICE_FEATURE_STATE_DISABLED:
  default:
    return QStringLiteral("disabled");
  }
}
} // namespace

QString ArtifactIRenderer::rayTracingDebugState() const
{
  if (!impl_ || !impl_->rayTracingManager_) {
    return QStringLiteral("<no rt manager>");
  }

  const auto caps = impl_->rayTracingManager_->capabilities();
  return QStringLiteral(
             "device=%1 feature=%2 supported=%3 unitQuadBLAS=%4 unitQuadBLASBuilt=%5 "
             "TLAS=%6 TLASBuilt=%7 blasBuilds=%8 tlasBuilds=%9 "
             "pipeline=%10 sbt=%11 sbtBound=%12 outputTexture=%13 outputBound=%14 "
             "traceDispatches=%15 lastTrace=%16x%17 "
             "maxDepth=%18 maxRayGen=%19 maxTLASInstances=%20 maxBLASPrimitives=%21 "
             "maxBLASGeometries=%22 scratchAlign=%23 instanceAlign=%24")
      .arg(rayTracingDeviceTypeName(caps.deviceType))
      .arg(rayTracingFeatureStateName(caps.featureState))
      .arg(caps.supported)
      .arg(caps.unitQuadBLASCreated)
      .arg(caps.unitQuadBLASBuilt)
      .arg(caps.tlasCreated)
      .arg(caps.tlasBuilt)
      .arg(caps.blasBuildCount)
      .arg(caps.tlasBuildCount)
      .arg(caps.pipelineCreated)
      .arg(caps.sbtCreated)
      .arg(caps.sbtBound)
      .arg(caps.outputTextureCreated)
      .arg(caps.outputResourcesBound)
      .arg(caps.traceDispatchCount)
      .arg(caps.lastTraceWidth)
      .arg(caps.lastTraceHeight)
      .arg(caps.maxRecursionDepth)
      .arg(caps.maxRayGenThreads)
      .arg(caps.maxInstancesPerTLAS)
      .arg(caps.maxPrimitivesPerBLAS)
      .arg(caps.maxGeometriesPerBLAS)
      .arg(caps.scratchBufferAlignment)
      .arg(caps.instanceBufferAlignment);
}
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
void ArtifactIRenderer::drawMesh(const QString& cacheKey, const ArtifactCore::Mesh& mesh,
                                 const ArtifactCore::Material& material,
                                 const QMatrix4x4& modelMatrix, float opacity)
{ impl_->drawMesh(cacheKey, mesh, material, modelMatrix, opacity); }
 void ArtifactIRenderer::setUpscaleConfig(bool enable, float sharpness)
 {
  impl_->m_upscaleEnabled = enable;
  impl_->m_upscaleSharpness = std::clamp(sharpness, 0.0f, 1.0f);
  // Higher sharpness means we keep a slightly larger internal render target.
  // 0.0 => 70% scale, 1.0 => 100% scale.
  impl_->m_upscaleScale = enable ? (0.70f + 0.30f * impl_->m_upscaleSharpness) : 1.0f;
  if (impl_->widget_) {
   impl_->createLayerRT(impl_->widget_);
  }
 }
 std::unique_ptr<ArtifactCore::LayerBlendPipeline>
 ArtifactIRenderer::createLayerBlendPipeline() const
 {
 auto device = impl_->deviceManager_.device();
 auto ctx = impl_->deviceManager_.immediateContext();
 if (!device || !ctx) {
  return {};
 }
 auto context = std::make_shared<ArtifactCore::GpuContext>(device, ctx);
 return std::make_unique<ArtifactCore::LayerBlendPipeline>(std::move(context));
}
bool ArtifactIRenderer::blendLayers(ArtifactCore::LayerBlendPipeline *pipeline,
                                    Diligent::ITextureView *srcSRV,
                                    Diligent::ITextureView *dstSRV,
                                    Diligent::ITextureView *outUAV,
                                    ArtifactCore::BlendMode mode,
                                    float opacity) const
 {
  auto ctx = impl_->deviceManager_.immediateContext();
  if (!pipeline || !ctx) {
   return false;
  }
  return pipeline->blend(ctx, srcSRV, dstSRV, outUAV, mode, opacity);
 }
bool ArtifactIRenderer::convertLayerToFloat(
    ArtifactCore::LayerBlendPipeline *pipeline,
    Diligent::ITextureView *srcSRV,
    Diligent::ITextureView *outUAV,
    Diligent::Uint32 width,
    Diligent::Uint32 height) const
{
 auto ctx = impl_->deviceManager_.immediateContext();
 if (!pipeline || !ctx) {
  return false;
 }
 return pipeline->convertLayerToFloat(ctx, srcSRV, outUAV, width, height);
}
 Diligent::RefCntAutoPtr<Diligent::IRenderDevice> ArtifactIRenderer::device() const
 { return impl_->deviceManager_.device(); }
 Diligent::RefCntAutoPtr<Diligent::IDeviceContext> ArtifactIRenderer::immediateContext() const
 { return impl_->deviceManager_.immediateContext(); }
 Diligent::ITextureView* ArtifactIRenderer::layerTextureView() const
 { return impl_->m_layerRT ? impl_->m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE) : nullptr; }
 Diligent::ITextureView* ArtifactIRenderer::layerRenderTargetView() const
 { return impl_->m_layerRT ? impl_->m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET) : nullptr; }
 Diligent::ITextureView* ArtifactIRenderer::rayTracingOutputTextureView() const
 { return impl_->rayTracingManager_ ? impl_->rayTracingManager_->traceOutputSRV() : nullptr; }
 ArtifactCore::IRayTracingManager* ArtifactIRenderer::rayTracingManager() const
 { return impl_->rayTracingManager_.get(); }
 void ArtifactIRenderer::setOverrideRTV(Diligent::ITextureView* rtv)
 {
  if (auto ctx = impl_->deviceManager_.immediateContext()) {
   impl_->submitQueuedDraws(ctx);
  }
  impl_->m_overrideColorRTV = rtv;
  impl_->primitiveRenderer_.setOverrideRTV(rtv);
  impl_->primitiveRenderer3D_.setOverrideRTV(rtv);
 }

 void ArtifactIRenderer::setOverrideDSV(Diligent::ITextureView* dsv)
 {
  if (auto ctx = impl_->deviceManager_.immediateContext()) {
   impl_->submitQueuedDraws(ctx);
  }
  impl_->m_overrideDepthDSV = dsv;
  impl_->primitiveRenderer3D_.setOverrideDSV(dsv);
 }

 // Offscreen rendering for group layers
 void* ArtifactIRenderer::createOffscreenTexture(int width, int height)
 {
  if (!impl_->deviceManager_.device()) return nullptr;

  Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
  Diligent::TextureDesc desc;
  desc.Name = "GroupOffscreenTexture";
  desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  desc.Width = static_cast<Diligent::Uint32>(width);
  desc.Height = static_cast<Diligent::Uint32>(height);
  desc.MipLevels = 1;
  desc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
  desc.Usage = Diligent::USAGE_DEFAULT;
  desc.BindFlags = Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

  impl_->deviceManager_.device()->CreateTexture(desc, nullptr, &texture);
  if (!texture) return nullptr;

  auto* view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  view->AddRef();
  return static_cast<void*>(view);
 }

 void* ArtifactIRenderer::createOffscreenDepthTexture(int width, int height)
 {
  if (!impl_->deviceManager_.device()) return nullptr;

  Diligent::RefCntAutoPtr<Diligent::ITexture> texture;
  Diligent::TextureDesc desc;
  desc.Name = "GroupOffscreenDepthTexture";
  desc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  desc.Width = static_cast<Diligent::Uint32>(width);
  desc.Height = static_cast<Diligent::Uint32>(height);
  desc.MipLevels = 1;
  desc.Format = Diligent::TEX_FORMAT_D32_FLOAT;
  desc.Usage = Diligent::USAGE_DEFAULT;
  desc.BindFlags = Diligent::BIND_DEPTH_STENCIL;

  impl_->deviceManager_.device()->CreateTexture(desc, nullptr, &texture);
  if (!texture) return nullptr;

  auto* view = texture->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
  if (!view) return nullptr;
  view->AddRef();
  return static_cast<void*>(view);
 }

 void ArtifactIRenderer::destroyOffscreenTexture(void* textureView)
 {
  if (textureView) {
   auto* view = static_cast<Diligent::ITextureView*>(textureView);
   view->Release();
  }
 }

 void ArtifactIRenderer::pushRenderTarget(void* textureView)
 {
  if (!textureView) return;
  auto* view = static_cast<Diligent::ITextureView*>(textureView);
   auto ctx = impl_->deviceManager_.immediateContext();
   if (ctx) {
    ctx->SetRenderTargets(1, &view, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
   }
 }

 void ArtifactIRenderer::popRenderTarget()
 {
   auto ctx = impl_->deviceManager_.immediateContext();
   if (ctx && impl_->m_layerRT) {
    auto* rtv = impl_->m_layerRT->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    ctx->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }
 }

 void ArtifactIRenderer::clearRenderTarget(const FloatColor& color)
 {
  auto ctx = impl_->deviceManager_.immediateContext();
  if (!ctx) return;
  auto* rtv = impl_->activeColorView();
  if (!rtv) return;
  const float clearColor[] = { color.r(), color.g(), color.b(), color.a() };
  ctx->SetRenderTargets(1, &rtv, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  ctx->ClearRenderTarget(rtv, clearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
 }

 void ArtifactIRenderer::drawOffscreenTexture(void* textureView, const QRectF& bounds, float opacity)
 {
  if (!textureView) return;
  auto* view = static_cast<Diligent::ITextureView*>(textureView);

  drawSprite(
   static_cast<float>(bounds.x()),
   static_cast<float>(bounds.y()),
   static_cast<float>(bounds.width()),
   static_cast<float>(bounds.height()),
   view,
   opacity
  );
 }

} // namespace Artifact
