module;
#include <DeviceContext.h>
#include <QImage>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <SwapChain.h>
#include <Texture.h>
#include <utility>


export module Artifact.Render.PrimitiveRenderer3D;

import std;
import Graphics;
import Color.Float;
import Artifact.Render.ShaderManager;

export namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class PrimitiveRenderer3D {
public:
  PrimitiveRenderer3D();
  ~PrimitiveRenderer3D();

  void createBuffers(RefCntAutoPtr<IRenderDevice> device);
  void createBuffers(RefCntAutoPtr<IRenderDevice> device,
                     TEXTURE_FORMAT rtvFormat);
  void setPSOs(ShaderManager &shaderManager);
  void setContext(IDeviceContext *ctx);
  void setContext(IDeviceContext *ctx, ISwapChain *swapChain);
  void setOverrideRTV(ITextureView *rtv);
  void destroy();

  void setViewMatrix(const QMatrix4x4 &view);
  void setProjectionMatrix(const QMatrix4x4 &proj);
  void setCameraMatrices(const QMatrix4x4 &view, const QMatrix4x4 &proj);
  void resetMatrices();

  void drawBillboardQuad(const QVector3D &center, const QVector2D &size,
                         const FloatColor &tint, float opacity = 1.0f,
                         float rollDegrees = 0.0f);
  void drawBillboardQuad(const QVector3D &center, const QVector2D &size,
                         ITextureView *texture,
                         const FloatColor &tint = FloatColor{1.0f, 1.0f, 1.0f,
                                                             1.0f},
                         float opacity = 1.0f, float rollDegrees = 0.0f);
  void drawBillboardQuad(const QVector3D &center, const QVector2D &size,
                         const QImage &image,
                         const FloatColor &tint = FloatColor{1.0f, 1.0f, 1.0f,
                                                             1.0f},
                         float opacity = 1.0f, float rollDegrees = 0.0f);
  void draw3DLine(const QVector3D &start, const QVector3D &end,
                  const FloatColor &color, float thickness = 1.0f);
  void draw3DArrow(const QVector3D &start, const QVector3D &end,
                   const FloatColor &color, float size = 1.0f);
  void draw3DCircle(const QVector3D &center, const QVector3D &normal,
                    float radius, const FloatColor &color,
                    float thickness = 1.0f);
  void draw3DQuad(const QVector3D &v0, const QVector3D &v1, const QVector3D &v2,
                  const QVector3D &v3, const FloatColor &color);
  void draw3DTorus(const QVector3D &center, const QVector3D &normal,
                   float majorRadius, float minorRadius,
                   const FloatColor &color);
  void draw3DCube(const QVector3D &center, float halfExtent,
                  const FloatColor &color);

  // Submit all accumulated 3D gizmo geometry to the GPU in one batch.
  // Must be called once after all draw3D*/drawGizmo* calls for the frame.
  void flushGizmo3D();

  // ImGuizmo 対応 追加関数
  void draw3DCircleArc(const QVector3D &center, const QVector3D &normal,
                       float radius, float startAngle, float endAngle,
                       const FloatColor &color, float thickness = 1.0f);

  void draw3DTriangle(const QVector3D &v0, const QVector3D &v1,
                      const QVector3D &v2, const FloatColor &color);

  void draw3DTriangleFan(const QVector3D *vertices, int vertexCount,
                         const FloatColor &color);

private:
  class Impl;
  Impl *impl_;
};

} // namespace Artifact
