module;
#include <algorithm>
#include <cmath>
#include <memory>

module Artifact.Effect.Rasterizer.Ripple;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;
import ImageProcessing.Distortion;
import Property.Abstract;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

namespace {

// Resident-path shader mirroring ArtifactCore::makeRipple. It relies on the
// shared generic parameter buffer (g_P0..g_P7) supplied by the render pipeline:
// P0 centerX, P1 centerY, P2 amplitude, P3 frequency, P4 decay, P5 phase.
static constexpr const char* kRippleResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float4 rippleSampleLinear(float2 position, uint width, uint height)
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
    if (dist < 0.001) {
        g_OutputTexture[dispatchId.xy] = g_InputTexture[dispatchId.xy];
        return;
    }
    const float falloff = exp(-g_P4 * dist);
    const float offset = sin(6.28318530718 * g_P3 * dist + g_P5) * g_P2 * falloff;
    g_OutputTexture[dispatchId.xy] =
        rippleSampleLinear(center + delta * ((dist + offset) / dist), width, height);
}
)";

} // namespace

class RippleEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float amplitude_ = 10.0f;
    float frequency_ = 0.01f;
    float decay_ = 0.005f;
    float phase_ = 0.0f;

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
        // The shared mapper works in pixel space, so convert the normalized
        // center and frequency once instead of per pixel.
        const float cx = centerX_ * static_cast<float>(width);
        const float cy = centerY_ * static_cast<float>(height);
        ImageF32x4_RGBA result;
        applyDisplacement(srcImage, result,
                          makeRipple(cx, cy, amplitude_, frequency_, decay_, phase_));
        dst.image().setFromRGBA32F(result.rgba32fData(), width, height,
                                   srcImage.colorDescriptor());
    }
};

class RippleEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float centerX_ = 0.5f;
    float centerY_ = 0.5f;
    float amplitude_ = 10.0f;
    float frequency_ = 0.01f;
    float decay_ = 0.005f;
    float phase_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
    RippleEffectCPUImpl cpuImpl_;
};

RippleEffect::RippleEffect() {
    setDisplayName(UniString("Ripple"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    auto cpu = makeShared<RippleEffectCPUImpl>();
    auto gpu = makeShared<RippleEffectGPUImpl>();
    setCPUImpl(cpu);
    setGPUImpl(gpu);
    setComputeMode(ComputeMode::AUTO);
    syncImpls();
    registerGpuGenericShader(
        RippleEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kRippleResidentHlsl, "main", GpuGenericResourceKind::Filter});
}

RippleEffect::~RippleEffect() = default;

float RippleEffect::centerX() const { return centerX_; }
void RippleEffect::setCenterX(float v) {
    centerX_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float RippleEffect::centerY() const { return centerY_; }
void RippleEffect::setCenterY(float v) {
    centerY_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float RippleEffect::amplitude() const { return amplitude_; }
void RippleEffect::setAmplitude(float v) {
    amplitude_ = std::isfinite(v) ? std::clamp(v, 0.0f, 4096.0f) : 10.0f;
    syncImpls();
}
float RippleEffect::frequency() const { return frequency_; }
void RippleEffect::setFrequency(float v) {
    frequency_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.01f;
    syncImpls();
}
float RippleEffect::decay() const { return decay_; }
void RippleEffect::setDecay(float v) {
    decay_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.005f;
    syncImpls();
}
float RippleEffect::phase() const { return phase_; }
void RippleEffect::setPhase(float v) {
    phase_ = std::isfinite(v) ? v : 0.0f;
    syncImpls();
}

void RippleEffect::syncImpls() {
    if (auto* c = dynamic_cast<RippleEffectCPUImpl*>(cpuImpl().get())) {
        c->centerX_ = centerX_;
        c->centerY_ = centerY_;
        c->amplitude_ = amplitude_;
        c->frequency_ = frequency_;
        c->decay_ = decay_;
        c->phase_ = phase_;
    }
    if (auto* g = dynamic_cast<RippleEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.centerX_ = centerX_;
        g->cpuImpl_.centerY_ = centerY_;
        g->cpuImpl_.amplitude_ = amplitude_;
        g->cpuImpl_.frequency_ = frequency_;
        g->cpuImpl_.decay_ = decay_;
        g->cpuImpl_.phase_ = phase_;
    }
}

std::vector<AbstractProperty> RippleEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& cx = props.emplace_back(); cx.setName("Center X"); cx.setType(PropertyType::Float); cx.setValue(centerX_); cx.setMinValue(QVariant(0.0)); cx.setMaxValue(QVariant(1.0));
    auto& cy = props.emplace_back(); cy.setName("Center Y"); cy.setType(PropertyType::Float); cy.setValue(centerY_); cy.setMinValue(QVariant(0.0)); cy.setMaxValue(QVariant(1.0));
    auto& a = props.emplace_back(); a.setName("Amplitude"); a.setType(PropertyType::Float); a.setValue(amplitude_); a.setMinValue(QVariant(0.0)); a.setMaxValue(QVariant(4096.0));
    auto& f = props.emplace_back(); f.setName("Frequency"); f.setType(PropertyType::Float); f.setValue(frequency_); f.setMinValue(QVariant(-1.0)); f.setMaxValue(QVariant(1.0));
    auto& d = props.emplace_back(); d.setName("Decay"); d.setType(PropertyType::Float); d.setValue(decay_); d.setMinValue(QVariant(0.0)); d.setMaxValue(QVariant(1.0));
    auto& p = props.emplace_back(); p.setName("Phase"); p.setType(PropertyType::Float); p.setValue(phase_); p.setMinValue(QVariant(-1000.0)); p.setMaxValue(QVariant(1000.0));
    return props;
}

void RippleEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString k = n.toQString();
    if (k == "Center X") setCenterX(v.toFloat());
    else if (k == "Center Y") setCenterY(v.toFloat());
    else if (k == "Amplitude") setAmplitude(v.toFloat());
    else if (k == "Frequency") setFrequency(v.toFloat());
    else if (k == "Decay") setDecay(v.toFloat());
    else if (k == "Phase") setPhase(v.toFloat());
}

} // namespace Artifact
