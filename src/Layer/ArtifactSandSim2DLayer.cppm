module;
#include <cmath>
#include <cstring>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <DiligentCore/Common/interface/RefCntAutoPtr.hpp>
#include <DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h>

module Artifact.Layer.SandSim2D;

import std;
import Artifact.Layer.Abstract;
import Artifact.Render.IRenderer;
import Artifact.Render.DiligentDeviceManager;
import Physics.SandSim2D;
import Graphics.SandGPUCompute;
import Graphics.GPUcomputeContext;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Property.Group;

namespace Artifact {

using namespace ArtifactCore;

namespace {

    // Color palette for sand materials
    FloatRGBA materialColor(SandMaterial mat, uint8_t life, int maxLife) {
        switch (mat) {
        case SandMaterial::Empty: return FloatRGBA(0, 0, 0, 0);
        case SandMaterial::Sand:  return FloatRGBA(0.76f, 0.70f, 0.50f, 1.0f);
        case SandMaterial::Water: return FloatRGBA(0.0f, 0.3f, 0.8f, 0.7f);
        case SandMaterial::Stone: return FloatRGBA(0.5f, 0.5f, 0.5f, 1.0f);
        case SandMaterial::Wood:  return FloatRGBA(0.55f, 0.27f, 0.07f, 1.0f);
        case SandMaterial::Fire: {
            float t = (maxLife > 0) ? static_cast<float>(life) / maxLife : 0.5f;
            float r = 1.0f;
            float g = t * 0.8f + 0.2f;
            float b = 0.0f;
            return FloatRGBA(r, g, b, 1.0f);
        }
        case SandMaterial::Smoke: {
            float alpha = (maxLife > 0) ? static_cast<float>(life) / maxLife * 0.7f : 0.5f;
            return FloatRGBA(0.15f, 0.15f, 0.15f, alpha);
        }
        case SandMaterial::Acid: return FloatRGBA(0.0f, 1.0f, 0.2f, 1.0f);
        }
        return FloatRGBA(0, 0, 0, 1);
    }

    constexpr int DEFAULT_SIM_SIZE = 160;
    constexpr int DEFAULT_TOOL_RADIUS = 3;

} // anonymous namespace

class ArtifactSandSim2DLayer::Impl {
public:
    Impl() : sim_(DEFAULT_SIM_SIZE, DEFAULT_SIM_SIZE)
           , toolMat_(SandMaterial::Sand)
           , toolRadius_(DEFAULT_TOOL_RADIUS)
           , simRes_(DEFAULT_SIM_SIZE)
           , needsRender_(true)
    {
    }

    bool tryInitGPU() {
        if (gpuCompute_) return true;
        RefCntAutoPtr<IRenderDevice> dev;
        RefCntAutoPtr<IDeviceContext> ctx;
        if (!acquireSharedRenderDeviceForCurrentBackend(dev, ctx)) return false;
        gpuContext_ = std::make_unique<GpuContext>(dev, ctx);
        device_ = dev;
        context_ = ctx;
        gpuCompute_ = std::make_unique<SandGPUCompute>(*gpuContext_);
        bool ok = gpuCompute_->initialize(sim_.width(), sim_.height());
        if (ok) {
            std::vector<uint8_t> gridData;
            gridData.reserve(sim_.grid().size());
            for (const auto cell : sim_.grid()) {
                gridData.push_back(static_cast<uint8_t>(cell));
            }
            gpuCompute_->uploadFromCPU(context_.RawPtr(), gridData, sim_.width(), sim_.height());
        }
        releaseSharedRenderDevice();
        return ok;
    }

    void uploadToGPU() {
        if (!gpuCompute_ || !context_) return;
        RefCntAutoPtr<IRenderDevice> dev;
        RefCntAutoPtr<IDeviceContext> ctx;
        if (!acquireSharedRenderDeviceForCurrentBackend(dev, ctx)) return;
        std::vector<uint8_t> gridData;
        gridData.reserve(sim_.grid().size());
        for (const auto cell : sim_.grid()) {
            gridData.push_back(static_cast<uint8_t>(cell));
        }
        gpuCompute_->uploadFromCPU(ctx.RawPtr(), gridData, sim_.width(), sim_.height());
        releaseSharedRenderDevice();
    }

    SandSim2D sim_;
    SandMaterial toolMat_ = SandMaterial::Sand;
    int toolRadius_ = DEFAULT_TOOL_RADIUS;
    int simRes_ = DEFAULT_SIM_SIZE;
    bool needsRender_ = true;
    bool paintPending_ = false;
    ImageF32x4_RGBA renderBuffer_;

    // GPU compute path
    std::unique_ptr<SandGPUCompute> gpuCompute_;
    std::unique_ptr<GpuContext> gpuContext_;
    RefCntAutoPtr<IRenderDevice> device_;
    RefCntAutoPtr<IDeviceContext> context_;
};

ArtifactSandSim2DLayer::ArtifactSandSim2DLayer()
    : impl_(new Impl())
{
    setSourceSize(Size_2D(640, 480));
}

ArtifactSandSim2DLayer::~ArtifactSandSim2DLayer()
{
    delete impl_;
}

void ArtifactSandSim2DLayer::draw(ArtifactIRenderer* renderer)
{
    int w = impl_->sim_.width();
    int h = impl_->sim_.height();

    // Rebuild render buffer if size changed
    if (impl_->renderBuffer_.width() != w || impl_->renderBuffer_.height() != h) {
        impl_->renderBuffer_.resize(w, h);
    }

    // Try GPU compute path if available
    bool gpuSimulated = false;
    if (impl_->tryInitGPU()) {
        RefCntAutoPtr<IRenderDevice> dev;
        RefCntAutoPtr<IDeviceContext> ctx;
        if (acquireSharedRenderDeviceForCurrentBackend(dev, ctx)) {
            // Sync pending CPU paint → GPU before simulation
            if (impl_->paintPending_) {
                std::vector<uint8_t> gridData;
                gridData.reserve(impl_->sim_.grid().size());
                for (const auto cell : impl_->sim_.grid()) {
                    gridData.push_back(static_cast<uint8_t>(cell));
                }
                impl_->gpuCompute_->uploadFromCPU(ctx.RawPtr(), gridData, w, h);
                impl_->paintPending_ = false;
            }

            // GPU simulate (2 substeps for smoother motion)
            impl_->gpuCompute_->simulate(ctx.RawPtr(), 2);

            // Readback GPU grid → CPU grid (keeps both in sync)
            std::vector<uint8_t> gpuGrid;
            impl_->gpuCompute_->readbackToCPU(ctx.RawPtr(), gpuGrid);

            releaseSharedRenderDevice();

            if (!gpuGrid.empty() && static_cast<int>(gpuGrid.size()) == w * h) {
                // Write GPU results back to CPU sim grid for sync
                for (int i = 0; i < w * h; ++i) {
                    impl_->sim_.setCell(i % w, i / w, static_cast<SandMaterial>(gpuGrid[i]));
                }

                // Render GPU grid to image buffer
                float* rgba = impl_->renderBuffer_.rgba32fData();
                for (int i = 0; i < w * h; ++i) {
                    FloatRGBA c = materialColor(static_cast<SandMaterial>(gpuGrid[i]), 0, 80);
                    rgba[i * 4 + 0] = c.r();
                    rgba[i * 4 + 1] = c.g();
                    rgba[i * 4 + 2] = c.b();
                    rgba[i * 4 + 3] = c.a();
                }
                gpuSimulated = true;
            }
        }
    }

    if (!gpuSimulated) {
        // CPU fallback: update simulation (1 substep)
        impl_->sim_.update(1);

        // Render CPU grid to image buffer
        float* rgba = impl_->renderBuffer_.rgba32fData();
        const auto& grid = impl_->sim_.grid();
        const auto& lifetime = impl_->sim_.lifetime();

        for (int i = 0; i < w * h; ++i) {
            FloatRGBA c = materialColor(grid[i], lifetime[i], 80);
            rgba[i * 4 + 0] = c.r();
            rgba[i * 4 + 1] = c.g();
            rgba[i * 4 + 2] = c.b();
            rgba[i * 4 + 3] = c.a();
        }
    }

    // Draw the rendered buffer through the renderer
    Size_2D srcSize = sourceSize();
    QMatrix4x4 transform = getGlobalTransform4x4();
    renderer->drawSpriteTransformed(0, 0, static_cast<float>(srcSize.width), static_cast<float>(srcSize.height),
                                     transform, impl_->renderBuffer_, opacity());
}

QImage ArtifactSandSim2DLayer::toQImage() const
{
    return impl_->renderBuffer_.toQImage();
}

QJsonObject ArtifactSandSim2DLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstractLayer::toJson();
    obj["simResolution"] = impl_->simRes_;
    obj["toolMaterial"] = static_cast<int>(impl_->toolMat_);
    obj["toolRadius"] = impl_->toolRadius_;
    return obj;
}

void ArtifactSandSim2DLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstractLayer::fromJsonProperties(obj);
    if (obj.contains("simResolution")) {
        setSimResolution(obj["simResolution"].toInt());
    }
    if (obj.contains("toolMaterial")) {
        impl_->toolMat_ = static_cast<SandMaterial>(obj["toolMaterial"].toInt());
    }
    if (obj.contains("toolRadius")) {
        impl_->toolRadius_ = obj["toolRadius"].toInt();
    }
}

std::shared_ptr<ArtifactSandSim2DLayer> ArtifactSandSim2DLayer::fromJson(const QJsonObject& obj)
{
    auto layer = std::make_shared<ArtifactSandSim2DLayer>();
    layer->fromJsonProperties(obj);
    return layer;
}

bool ArtifactSandSim2DLayer::isAdjustmentLayer() const
{
    return false;
}

bool ArtifactSandSim2DLayer::isNullLayer() const
{
    return false;
}

std::vector<ArtifactCore::PropertyGroup> ArtifactSandSim2DLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    ArtifactCore::PropertyGroup simGroup(QStringLiteral("Sand Simulation"));
    simGroup.addProperty(persistentLayerProperty(
        QStringLiteral("sandSim.resolution"),
        ArtifactCore::PropertyType::Integer,
        QVariant(impl_->simRes_), 0));
    simGroup.addProperty(persistentLayerProperty(
        QStringLiteral("sandSim.toolMaterial"),
        ArtifactCore::PropertyType::Integer,
        QVariant(static_cast<int>(impl_->toolMat_)), 0));
    simGroup.addProperty(persistentLayerProperty(
        QStringLiteral("sandSim.toolRadius"),
        ArtifactCore::PropertyType::Integer,
        QVariant(impl_->toolRadius_), 0));
    groups.push_back(simGroup);

    return groups;
}

bool ArtifactSandSim2DLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == QStringLiteral("sandSim.resolution")) {
        setSimResolution(value.toInt());
        return true;
    }
    if (propertyPath == QStringLiteral("sandSim.toolMaterial")) {
        impl_->toolMat_ = static_cast<SandMaterial>(value.toInt());
        return true;
    }
    if (propertyPath == QStringLiteral("sandSim.toolRadius")) {
        impl_->toolRadius_ = value.toInt();
        return true;
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

// Sand simulation controls
void ArtifactSandSim2DLayer::setSimResolution(int cellsPerDimension)
{
    int res = std::clamp(cellsPerDimension, 32, 512);
    if (res != impl_->simRes_) {
        impl_->simRes_ = res;
        int aspectW = sourceSize().width;
        int aspectH = sourceSize().height;
        if (aspectW > aspectH) {
            impl_->sim_ = SandSim2D(res, std::max(1, res * aspectH / aspectW));
        } else {
            impl_->sim_ = SandSim2D(std::max(1, res * aspectW / aspectH), res);
        }
        impl_->needsRender_ = true;
        impl_->paintPending_ = true;
        // Reset GPU compute so it re-initializes on next frame
        impl_->gpuCompute_.reset();
        impl_->gpuContext_.reset();
    }
}

int ArtifactSandSim2DLayer::simResolution() const
{
    return impl_->simRes_;
}

void ArtifactSandSim2DLayer::setToolMaterial(ArtifactCore::SandMaterial mat)
{
    impl_->toolMat_ = mat;
}

ArtifactCore::SandMaterial ArtifactSandSim2DLayer::toolMaterial() const
{
    return impl_->toolMat_;
}

void ArtifactSandSim2DLayer::setToolRadius(int radius)
{
    impl_->toolRadius_ = std::clamp(radius, 1, 50);
}

int ArtifactSandSim2DLayer::toolRadius() const
{
    return impl_->toolRadius_;
}

void ArtifactSandSim2DLayer::paintAt(int x, int y)
{
    // Map screen coordinates to sim grid coordinates
    Size_2D srcSize = sourceSize();
    if (srcSize.width <= 0 || srcSize.height <= 0) return;

    float sx = static_cast<float>(x) / srcSize.width * impl_->sim_.width();
    float sy = static_cast<float>(y) / srcSize.height * impl_->sim_.height();

    impl_->sim_.fillCircle(static_cast<int>(sx), static_cast<int>(sy),
                           impl_->toolRadius_, impl_->toolMat_);
    impl_->paintPending_ = true;
}

void ArtifactSandSim2DLayer::clearSim()
{
    impl_->sim_.clear();
    impl_->paintPending_ = true;
}

ArtifactCore::SandSim2D* ArtifactSandSim2DLayer::simulation()
{
    return &impl_->sim_;
}

} // namespace Artifact
