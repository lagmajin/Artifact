module;
#include <algorithm>
#include <cmath>
#include <memory>

module Artifact.Effect.Rasterizer.PolarCoordinates;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;
import Property.Abstract;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

namespace {

// Resident-path shader mirroring ArtifactCore::makePolarCoordinates. Shared
// generic parameter buffer: P0 centerX, P1 centerY, P2 radius (fraction of the
// smaller image side), P3 amount (0 keeps the image, 1 fully remaps it).
static constexpr const char* kPolarCoordinatesResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float4 polarSampleLinear(float2 position, uint width, uint height)
{
    position = clamp(position, float2(0.0, 0.0),
                     float2(width - 1, height - 1));
    const int2 lower = int2(floor(position));
    const float2 fraction = frac(position);
    const int2 upperX = min(lower + int2(1, 0), int2(width - 1, height - 1));
    const int2 upperY = min(lower + int2(0, 1), int2(width - 1, height - 1));
    const int2 upperXY = min(lower + int2(1, 1), int2(width - 1, height - 1));
    return lerp(lerp(g_InputTexture[uint2(lower)], g_InputTexture[uint2(upperX)], fraction.x),
                lerp(g_InputTexture[uint2(upperY)], g_InputTexture[uint2(upperXY)], fraction.x),
                fraction.y);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const float t = clamp(g_P3, 0.0, 1.0);
    if (t <= 0.0) {
        g_OutputTexture[dispatchId.xy] = g_InputTexture[dispatchId.xy];
        return;
    }
    const float2 center = float2(g_P0, g_P1) * float2(width, height);
    const float2 delta = float2(dispatchId.xy) - center;
    const float dist = length(delta);
    if (dist < 0.001) {
        g_OutputTexture[dispatchId.xy] = g_InputTexture[dispatchId.xy];
        return;
    }
    const float maxRadius = max(min(width, height) * g_P2, 0.001);
    const float angle = atan2(delta.y, delta.x);
    const float normAngle = (angle + 3.14159265359) / 6.28318530718;
    const float sourceRadius = maxRadius * normAngle;
    const float sourceDistance = dist * (1.0 - t) + sourceRadius * t;
    g_OutputTexture[dispatchId.xy] =
        polarSampleLinear(center + delta * (sourceDistance / dist), width, height);
}
)";

} // namespace

class PolarCoordinatesEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        const ImageF32x4_RGBA& srcImage = src.image();
        if (srcImage.rgba32fData() == nullptr) {
            dst = src;
            return;
        }
        const int width = srcImage.width();
        const int height = srcImage.height();
        if (width <= 0 || height <= 0) {
            dst = src;
            return;
        }
        const float cx = centerX_ * static_cast<float>(width);
        const float cy = centerY_ * static_cast<float>(height);
        const float maxRadius =
            std::min(static_cast<float>(width), static_cast<float>(height)) * radius_;
        if (maxRadius <= 0.0f) {
            dst = src;
            return;
        }
        ImageF32x4_RGBA result;
        applyDisplacement(srcImage, result,
                          makePolarCoordinates(cx, cy, maxRadius, amount_));
        dst.image().setFromRGBA32F(result.rgba32fData(), width, height,
                                   srcImage.colorDescriptor());
    }
};

class PolarCoordinatesEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 1.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    PolarCoordinatesEffectCPUImpl cpuImpl_;
};

PolarCoordinatesEffect::PolarCoordinatesEffect() {
    setDisplayName(UniString("Polar Coordinates"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    auto cpu = makeShared<PolarCoordinatesEffectCPUImpl>();
    auto gpu = makeShared<PolarCoordinatesEffectGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
    registerGpuGenericShader(
        PolarCoordinatesEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kPolarCoordinatesResidentHlsl, "main", GpuGenericResourceKind::Filter});
}

PolarCoordinatesEffect::~PolarCoordinatesEffect() = default;

float PolarCoordinatesEffect::centerX() const { return centerX_; }
void PolarCoordinatesEffect::setCenterX(float v) {
    centerX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PolarCoordinatesEffect::centerY() const { return centerY_; }
void PolarCoordinatesEffect::setCenterY(float v) {
    centerY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PolarCoordinatesEffect::radius() const { return radius_; }
void PolarCoordinatesEffect::setRadius(float v) {
    radius_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PolarCoordinatesEffect::amount() const { return amount_; }
void PolarCoordinatesEffect::setAmount(float v) {
    amount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
    syncImpls();
}

void PolarCoordinatesEffect::syncImpls() {
    if (auto* c = dynamic_cast<PolarCoordinatesEffectCPUImpl*>(cpuImpl().get())) {
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->radius_ = radius_;
        c->amount_ = amount_;
    }
    if (auto* g = dynamic_cast<PolarCoordinatesEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.radius_ = radius_;
        g->cpuImpl_.amount_ = amount_;
    }
}

std::vector<AbstractProperty> PolarCoordinatesEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_); cx.setMinValue(QVariant(0.0)); cx.setMaxValue(QVariant(1.0));
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_); cy.setMinValue(QVariant(0.0)); cy.setMaxValue(QVariant(1.0));
    auto& r = props.emplace_back(); r.setName("Radius"); r.setType(PropertyType::Float); r.setValue(radius_); r.setMinValue(QVariant(0.0)); r.setMaxValue(QVariant(1.0));
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_); a.setMinValue(QVariant(0.0)); a.setMaxValue(QVariant(1.0));
    return props;
}

void PolarCoordinatesEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Radius") setRadius(v.toFloat());
    else if (k == "Amount") setAmount(v.toFloat());
}

} // namespace Artifact
