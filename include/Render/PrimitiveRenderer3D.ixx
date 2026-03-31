module;
#include <QImage>
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <RenderDevice.h>
#include <DeviceContext.h>
#include <SwapChain.h>
#include <Texture.h>
#include <RefCntAutoPtr.hpp>

export module Artifact.Render.PrimitiveRenderer3D;

import Graphics;
import Color.Float;

export namespace Artifact {

using namespace Diligent;
using namespace ArtifactCore;

class ShaderManager;

class PrimitiveRenderer3D {
public:
    PrimitiveRenderer3D();
    ~PrimitiveRenderer3D();

    void createBuffers(RefCntAutoPtr<IRenderDevice> device);
    void createBuffers(RefCntAutoPtr<IRenderDevice> device, TEXTURE_FORMAT rtvFormat);
    void setPSOs(ShaderManager& shaderManager);
    void setContext(IDeviceContext* ctx);
    void setContext(IDeviceContext* ctx, ISwapChain* swapChain);
    void setOverrideRTV(ITextureView* rtv);
    void destroy();

    void setViewMatrix(const QMatrix4x4& view);
    void setProjectionMatrix(const QMatrix4x4& proj);
    void setCameraMatrices(const QMatrix4x4& view, const QMatrix4x4& proj);
    void resetMatrices();

    void drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                           const FloatColor& tint, float opacity = 1.0f,
                           float rollDegrees = 0.0f);
    void drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                           ITextureView* texture, const FloatColor& tint = FloatColor{1.0f, 1.0f, 1.0f, 1.0f},
                           float opacity = 1.0f, float rollDegrees = 0.0f);
    void drawBillboardQuad(const QVector3D& center, const QVector2D& size,
                           const QImage& image, const FloatColor& tint = FloatColor{1.0f, 1.0f, 1.0f, 1.0f},
                           float opacity = 1.0f, float rollDegrees = 0.0f);
    void draw3DLine(const QVector3D& start, const QVector3D& end,
                    const FloatColor& color, float thickness = 1.0f);
    void draw3DArrow(const QVector3D& start, const QVector3D& end,
                     const FloatColor& color, float size = 1.0f);
    void draw3DCircle(const QVector3D& center, const QVector3D& normal,
                      float radius, const FloatColor& color, float thickness = 1.0f);
    void draw3DQuad(const QVector3D& v0, const QVector3D& v1, const QVector3D& v2, const QVector3D& v3,
                    const FloatColor& color);

private:
    class Impl;
    Impl* impl_;
};

}
