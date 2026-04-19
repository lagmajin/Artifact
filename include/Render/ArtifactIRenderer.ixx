module;
#include <utility>
// ArtifactIRenderer maintenance rule:
// Do not rewrite the existing D3D12-specific path by guesswork.
// Do not replace this renderer with a Qt-only implementation.
// Extend backends carefully while preserving the current Diligent/D3D12
// architecture.
#include <DeviceContext.h>
#include <QFont>
#include <QImage>
#include <QMatrix4x4>
#include <QRectF>
#include <QString>
#include <QTransform>
#include <QWidget>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <functional>
#include <memory>

export module Artifact.Render.IRenderer;

import Color.Float;
export import Image.MultiChannelImage;
import Graphics.RayTracingManager;
import Graphics.ParticleData;
import Core.Light;
import Artifact.LOD.Manager;

export namespace Artifact {
using namespace ArtifactCore;

export namespace Detail {
export struct RenderShaderPair {
  void *VS = nullptr;
  void *PS = nullptr;
};

export struct PSOAndSRB {
  void *pPSO = nullptr;
  void *pSRB = nullptr;
};

export struct float2 {
  float x = 0.0f;
  float y = 0.0f;
  float2() = default;
  float2(float x_, float y_) : x(x_), y(y_) {}
};

export struct float3 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float3() = default;
  float3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};
} // namespace Detail

export class ArtifactIRenderer {
public:
  class Impl;

  ArtifactIRenderer();
  explicit ArtifactIRenderer(
      Diligent::RefCntAutoPtr<Diligent::IRenderDevice> pDevice,
      Diligent::RefCntAutoPtr<Diligent::IDeviceContext> pContext,
      QWidget *widget);
  ~ArtifactIRenderer();

  void initialize(QWidget *widget);
  void initializeHeadless(int width, int height);
  void createSwapChain(QWidget *widget);
  void recreateSwapChain(QWidget *widget);

  void clear();
  void flush();
  void flushAndWait();
  void destroy();
  void present();
  bool isInitialized() const;
  bool hasSwapChain() const;

  QImage readbackToImage() const;
  QImage readbackDepthToImage() const;
  ArtifactCore::MultiChannelImage readbackToMultiChannelImage() const;

  // Async readback: returns immediately, calls callback when ready
  using ReadbackCallback = std::function<void(const QImage &)>;
  void readbackToImageAsync(ReadbackCallback callback) const;

  void setClearColor(const FloatColor &color);
  FloatColor getClearColor() const;
  void setViewportSize(float w, float h);

  // ==== マルチチャンネルレンダリング ====
  void setMultiChannelEnabled(bool enabled);
  bool isMultiChannelEnabled() const;

  enum class ChannelType {
    Red,
    Green,
    Blue,
    Alpha,
    Depth,
    NormalX,
    NormalY,
    NormalZ,
    VelocityX,
    VelocityY,
    ObjectId,
    MaterialId,
    Emission,
    Custom
  };

  void setChannelEnabled(ChannelType channel, bool enabled);
  bool isChannelEnabled(ChannelType channel) const;
  void setCanvasSize(float w, float h);
  void setPan(float x, float y);
  void getPan(float &x, float &y) const;
  void setZoom(float zoom);
  float getZoom() const;
  void panBy(float dx, float dy);
  void resetView();
  void fitToViewport(float margin = 50.0f);
  void fillToViewport(float margin = 0.0f);
  void setViewMatrix(const QMatrix4x4 &view);
  void setProjectionMatrix(const QMatrix4x4 &proj);
  void setUseExternalMatrices(bool use);
  void setGizmoCameraMatrices(const QMatrix4x4 &view, const QMatrix4x4 &proj);
  void resetGizmoCameraMatrices();
  void set3DCameraMatrices(const QMatrix4x4 &view, const QMatrix4x4 &proj);
  void reset3DCameraMatrices();
  QMatrix4x4 getViewMatrix() const;
  QMatrix4x4 getProjectionMatrix() const;
  void zoomAroundViewportPoint(Detail::float2 viewportPos, float newZoom);

  Detail::float2 canvasToViewport(Detail::float2 pos) const;
  Detail::float2 viewportToCanvas(Detail::float2 pos) const;

  // LOD (Level of Detail)
  void setDetailLevel(LODManager::DetailLevel lod);
  LODManager::DetailLevel detailLevel() const;

  void drawRectOutline(float x, float y, float w, float h,
                       const FloatColor &color);
  void drawRectOutline(Detail::float2 pos, Detail::float2 size,
                       const FloatColor &color);
  void drawSolidLine(Detail::float2 start, Detail::float2 end,
                     const FloatColor &color, float thickness);
  void drawPolyline(const std::vector<Detail::float2> &points,
                    const FloatColor &color, float thickness);
  void drawQuadLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2,
                     Detail::float2 p3, const FloatColor &color);
  void drawSolidRect(float x, float y, float w, float h);
  void drawSolidRect(float x, float y, float w, float h,
                     const FloatColor &color, float opacity = 1.0f);
  void drawSolidRect(Detail::float2 pos, Detail::float2 size,
                     const FloatColor &color, float opacity = 1.0f);
  void drawPoint(float x, float y, float size, const FloatColor &color);
  void drawParticles(const ArtifactCore::ParticleRenderData &data);
  void drawSprite(float x, float y, float w, float h);
  void drawSprite(Detail::float2 pos, Detail::float2 size);
  void drawSprite(float x, float y, float w, float h,
                  Diligent::ITextureView *pSRV, float opacity = 1.0f);
  void drawSprite(float x, float y, float w, float h, const QImage &image,
                  float opacity = 1.0f);
  void drawText(const QRectF &rect, const QString &text, const QFont &font,
                const FloatColor &color,
                Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop,
                float opacity = 1.0f);
  void drawTextTransformed(const QRectF &rect, const QString &text,
                           const QFont &font, const FloatColor &color,
                           const QMatrix4x4 &transform,
                           Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignTop,
                           float opacity = 1.0f);
  void drawSpriteTransformed(float x, float y, float w, float h,
                             const QTransform &transform, const QImage &image,
                             float opacity = 1.0f);
  void drawSpriteRotated(float x, float y, float w, float h, float angleDegrees,
                         const QImage &image, float opacity = 1.0f);
  void drawSpriteTransformed(float x, float y, float w, float h,
                             const QMatrix4x4 &transform, const QImage &image,
                             float opacity = 1.0f);
  void drawSpriteTransformed(float x, float y, float w, float h,
                             const QMatrix4x4 &transform,
                             Diligent::ITextureView *texture,
                             float opacity = 1.0f);
  void drawMaskedTextureLocal(float x, float y, float w, float h,
                              Diligent::ITextureView *sceneTexture,
                              const QImage &maskImage, float opacity = 1.0f);
  void drawRectLocal(float x, float y, float w, float h,
                     const FloatColor &color, float opacity = 1.0f);
  void drawSolidRectTransformed(float x, float y, float w, float h,
                                const QTransform &transform,
                                const FloatColor &color, float opacity = 1.0f);
  void drawSolidRectTransformed(float x, float y, float w, float h,
                                const QMatrix4x4 &transform,
                                const FloatColor &color, float opacity = 1.0f);
  void drawRectOutlineLocal(float x, float y, float w, float h,
                            const FloatColor &color);
  void drawThickLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness,
                          const FloatColor &color);
  void drawDotLineLocal(Detail::float2 p1, Detail::float2 p2, float thickness,
                        float spacing, const FloatColor &color);
  void drawDashedLineLocal(Detail::float2 p1, Detail::float2 p2,
                           float thickness, float dashLength, float gapLength,
                           const FloatColor &color);
  void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2,
                       float thickness, const FloatColor &color);
  void drawBezierLocal(Detail::float2 p0, Detail::float2 p1, Detail::float2 p2,
                       Detail::float2 p3, float thickness,
                       const FloatColor &color);
  void drawSolidTriangleLocal(Detail::float2 p0, Detail::float2 p1,
                              Detail::float2 p2, const FloatColor &color);

  // Gizmo specialized APIs
  void drawCircle(float x, float y, float radius, const FloatColor &color,
                  float thickness = 1.0f, bool fill = false);
  void drawCrosshair(float x, float y, float size, const FloatColor &color);

  // 3D Gizmo APIs
  // These are viewport-manipulator primitives, not literal 3D mesh lines.
  void drawGizmoLine(Detail::float3 start, Detail::float3 end,
                     const FloatColor &color, float thickness = 1.0f);
  void drawGizmoArrow(Detail::float3 start, Detail::float3 end,
                      const FloatColor &color, float size = 1.0f);
  void drawGizmoRing(Detail::float3 center, Detail::float3 normal, float radius,
                     const FloatColor &color, float thickness = 1.0f);
  void drawGizmoTorus(Detail::float3 center, Detail::float3 normal,
                      float majorRadius, float minorRadius,
                      const FloatColor &color);
  void drawGizmoCube(Detail::float3 center, float halfExtent,
                     const FloatColor &color);
  // Submit all accumulated 3D gizmo geometry in one batch draw call.
  // Call once after all drawGizmo*/draw3D* calls for a frame.
  void flushGizmo3D();
  void draw3DLine(Detail::float3 start, Detail::float3 end,
                  const FloatColor &color, float thickness = 1.0f);
  void draw3DArrow(Detail::float3 start, Detail::float3 end,
                   const FloatColor &color, float size = 1.0f);
  void draw3DCircle(Detail::float3 center, Detail::float3 normal, float radius,
                    const FloatColor &color, float thickness = 1.0f);
  void draw3DQuad(Detail::float3 v0, Detail::float3 v1, Detail::float3 v2,
                  Detail::float3 v3, const FloatColor &color);

  void drawCheckerboard(float x, float y, float w, float h, float tileSize,
                        const FloatColor &c1, const FloatColor &c2);

  // Offscreen rendering for group layers
  void *createOffscreenTexture(int width, int height);
  void destroyOffscreenTexture(void *textureView);
  void pushRenderTarget(void *textureView);
  void popRenderTarget();
  void clearRenderTarget(const FloatColor &color);
  void drawOffscreenTexture(void *textureView, const QRectF &bounds,
                            float opacity = 1.0f);
  void drawGrid(float x, float y, float w, float h, float spacing,
                float thickness, const FloatColor &color);
  void setUpscaleConfig(bool enable, float sharpness);

  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device() const;
  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> immediateContext() const;
  Diligent::ITextureView *layerTextureView() const;
  Diligent::ITextureView *layerRenderTargetView() const;
  ArtifactCore::IRayTracingManager *rayTracingManager() const;
  void setOverrideRTV(Diligent::ITextureView *rtv);

  void setSceneLights(const std::vector<ArtifactCore::Light> &lights);
  const std::vector<ArtifactCore::Light> &getSceneLights() const;

private:
  std::unique_ptr<Impl> impl_;
};

} // namespace Artifact
