module;
#include <algorithm>
#include <cmath>
#include <memory>

module Artifact.Effect.Rasterizer.PinchBulge;

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

// Resident-path shader mirroring ArtifactCore::makePinchBulge. Shared generic
// parameter buffer: P0 centerX, P1 centerY, P2 radius (fraction of the smaller
// image side), P3 amount (positive bulges outward, negative pinches inward).
static constexpr const char* kPinchBulgeResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float4 pinchBulgeSampleLinear(float2 position, uint width, uint height)
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

    const float2 center = float2(g_P0, g_P1) * float2(width, height);
    const float2 delta = float2(dispatchId.xy) - center;
    const float dist = length(delta);
    const float maxRadius = max(min(width, height) * g_P2, 0.001);
    if (dist < 0.001 || dist > maxRadius) {
        g_OutputTexture[dispatchId.xy] = g_InputTexture[dispatchId.xy];
        return;
    }
    const float t = dist / maxRadius;
    const float scaled = pow(t, 1.0 + g_P3 * 0.01) * maxRadius;
    g_OutputTexture[dispatchId.xy] =
        pinchBulgeSampleLinear(center + delta * (scaled / dist), width, height);
}
)";

} // namespace

class PinchBulgeEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 50.0f;

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
        applyDisplacement(srcImage, result, makePinchBulge(cx, cy, maxRadius, amount_));
        dst.image().setFromRGBA32F(result.rgba32fData(), width, height,
                                   srcImage.colorDescriptor());
    }
};

class PinchBulgeEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float radius_ = 0.5f;
    float amount_ = 50.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    PinchBulgeEffectCPUImpl cpuImpl_;
};

PinchBulgeEffect::PinchBulgeEffect() {
    setDisplayName(UniString("Pinch / Bulge"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    auto cpu = makeShared<PinchBulgeEffectCPUImpl>();
    auto gpu = makeShared<PinchBulgeEffectGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
    registerGpuGenericShader(
        PinchBulgeEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kPinchBulgeResidentHlsl, "main", GpuGenericResourceKind::Filter});
}

PinchBulgeEffect::~PinchBulgeEffect() = default;

float PinchBulgeEffect::centerX() const { return centerX_; }
void PinchBulgeEffect::setCenterX(float v) {
    centerX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PinchBulgeEffect::centerY() const { return centerY_; }
void PinchBulgeEffect::setCenterY(float v) {
    centerY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PinchBulgeEffect::radius() const { return radius_; }
void PinchBulgeEffect::setRadius(float v) {
    radius_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float PinchBulgeEffect::amount() const { return amount_; }
void PinchBulgeEffect::setAmount(float v) {
    amount_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 50.0f;
    syncImpls();
}

void PinchBulgeEffect::syncImpls() {
    if (auto* c = dynamic_cast<PinchBulgeEffectCPUImpl*>(cpuImpl().get())) {
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->radius_ = radius_;
        c->amount_ = amount_;
    }
    if (auto* g = dynamic_cast<PinchBulgeEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.radius_ = radius_;
        g->cpuImpl_.amount_ = amount_;
    }
}

std::vector<AbstractProperty> PinchBulgeEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_); cx.setMinValue(QVariant(0.0)); cx.setMaxValue(QVariant(1.0));
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_); cy.setMinValue(QVariant(0.0)); cy.setMaxValue(QVariant(1.0));
    auto& r = props.emplace_back(); r.setName("Radius"); r.setType(PropertyType::Float); r.setValue(radius_); r.setMinValue(QVariant(0.0)); r.setMaxValue(QVariant(1.0));
    auto& a = props.emplace_back(); a.setName("Amount"); a.setType(PropertyType::Float); a.setValue(amount_); a.setMinValue(QVariant(-100.0)); a.setMaxValue(QVariant(100.0));
    return props;
}

void PinchBulgeEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Radius") setRadius(v.toFloat());
    else if (k == "Amount") setAmount(v.toFloat());
}

} // namespace Artifact
