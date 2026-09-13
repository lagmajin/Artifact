module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Glow.LiquidGlow;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

namespace {

int kernelSizeForRadius(float radius)
{
    const int halfWidth = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    return halfWidth * 2 + 1;
}

class LiquidGlowEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float threshold = 0.55f;
    float radius = 16.0f;
    float intensity = 1.1f;
    float flowScale = 42.0f;
    float distortion = 8.0f;
    float phase = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override
    {
        const auto& source = src.image();
        const float* sourcePixels = source.rgba32fData();
        if (!sourcePixels || source.width() <= 0 || source.height() <= 0) {
            dst = src.DeepCopy();
            return;
        }

        cv::Mat sourceMat(source.height(), source.width(), CV_32FC4,
                          const_cast<float*>(sourcePixels));
        std::vector<cv::Mat> channels;
        cv::split(sourceMat, channels);

        cv::Mat luminance =
            channels[0] * 0.114f + channels[1] * 0.587f + channels[2] * 0.299f;
        cv::Mat brightMask;
        cv::subtract(luminance, cv::Scalar::all(threshold), brightMask);
        cv::threshold(brightMask, brightMask, 0.0, 0.0, cv::THRESH_TOZERO);
        if (threshold < 0.999f) {
            brightMask *= 1.0f / std::max(0.001f, 1.0f - threshold);
        }

        cv::Mat color;
        cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, color);
        cv::Mat mask3;
        cv::merge(std::vector<cv::Mat>{brightMask, brightMask, brightMask}, mask3);
        cv::Mat glow = color.mul(mask3);
        cv::GaussianBlur(glow, glow,
                         cv::Size(kernelSizeForRadius(radius), kernelSizeForRadius(radius)),
                         std::max(0.1f, radius), std::max(0.1f, radius),
                         cv::BORDER_REPLICATE);

        cv::Mat mapX(source.height(), source.width(), CV_32FC1);
        cv::Mat mapY(source.height(), source.width(), CV_32FC1);
        const float scale = std::max(4.0f, flowScale);
        ArtifactCore::Parallel::For(0, source.height(), source.width() * source.height(), [&](int y) {
            float* xRow = mapX.ptr<float>(y);
            float* yRow = mapY.ptr<float>(y);
            for (int x = 0; x < source.width(); ++x) {
                const float nx = static_cast<float>(x) / scale;
                const float ny = static_cast<float>(y) / scale;
                const float flowX =
                    std::sin(ny * 1.73f + phase) +
                    std::sin((nx + ny) * 0.71f - phase * 0.63f) * 0.5f;
                const float flowY =
                    std::cos(nx * 1.37f - phase * 0.81f) +
                    std::cos((nx - ny) * 0.83f + phase) * 0.5f;
                xRow[x] = static_cast<float>(x) + flowX * distortion;
                yRow[x] = static_cast<float>(y) + flowY * distortion;
            }
        });

        cv::Mat flowedGlow(glow.rows, glow.cols, glow.type());
        auto reflect101 = [](int v, int limit) {
            if (limit <= 1) return 0;
            while (v < 0 || v >= limit) {
                if (v < 0) v = -v;
                if (v >= limit) v = 2 * limit - v - 2;
            }
            return v;
        };
        const cv::Vec3f* glowData = glow.ptr<cv::Vec3f>(0);
        ArtifactCore::Parallel::For(0, glow.rows, glow.rows * glow.cols, [&](int y) {
            const float* xRow = mapX.ptr<float>(y);
            const float* yRow = mapY.ptr<float>(y);
            cv::Vec3f* output = flowedGlow.ptr<cv::Vec3f>(y);
            for (int x = 0; x < glow.cols; ++x) {
                const float sx = xRow[x];
                const float sy = yRow[x];
                const int x0 = static_cast<int>(std::floor(sx));
                const int y0 = static_cast<int>(std::floor(sy));
                const int x1 = x0 + 1;
                const int y1 = y0 + 1;
                const float tx = sx - static_cast<float>(x0);
                const float ty = sy - static_cast<float>(y0);
                const cv::Vec3f& p00 = glowData[reflect101(y0, glow.rows) * glow.cols + reflect101(x0, glow.cols)];
                const cv::Vec3f& p10 = glowData[reflect101(y0, glow.rows) * glow.cols + reflect101(x1, glow.cols)];
                const cv::Vec3f& p01 = glowData[reflect101(y1, glow.rows) * glow.cols + reflect101(x0, glow.cols)];
                const cv::Vec3f& p11 = glowData[reflect101(y1, glow.rows) * glow.cols + reflect101(x1, glow.cols)];
                output[x] = p00 * ((1.0f - tx) * (1.0f - ty)) +
                            p10 * (tx * (1.0f - ty)) +
                            p01 * ((1.0f - tx) * ty) + p11 * (tx * ty);
            }
        });

        cv::Mat result = sourceMat.clone();
        std::vector<cv::Mat> flowedChannels;
        cv::split(flowedGlow, flowedChannels);
        for (int channel = 0; channel < 3; ++channel) {
            cv::Mat combined = channels[channel] + flowedChannels[channel] * intensity;
            cv::min(combined, 1.0, channels[channel]);
        }
        cv::merge(channels, result);
        dst.image().setFromRGBA32F(
            result.ptr<float>(), result.cols, result.rows,
            src.image().colorDescriptor());
    }
};

} // namespace

// Resident-path HLSL. Mirrors LiquidGlowEffectCPUImpl: threshold mask,
// separable Gaussian (OpenCV kernel half-width ceil(radius*2)), flow-field
// remap with bilinear sampling, additive composite clamped at 1.
// Approximations (tolerance-gated parity required):
// - Blur loop radius clamped to 16 taps per direction, and the effect only
//   contributes a resident node for radius <= 8 (see appendGpuSpatialNodes).
//   Larger radii stay on the CPU reference path.
// - Remap border uses clamp instead of the CPU reflect101 mirror; only the
//   outer distortion band (<= distortion px) can differ.
static constexpr const char* kLiquidGlowResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

float3 liquidGlowMasked(int2 p, uint w, uint h)
{
    p.x = clamp(p.x, 0, (int)w - 1);
    p.y = clamp(p.y, 0, (int)h - 1);
    float4 s = g_InputTexture[uint2(p)];
    float lum = s.r * 0.114f + s.g * 0.587f + s.b * 0.299f;
    float bright = max(lum - g_P0, 0.0f);
    if (g_P0 < 0.999f) bright /= max(0.001f, 1.0f - g_P0);
    return s.rgb * bright;
}

float3 liquidGlowHBlur(int x, int y, uint w, uint h, float sigma, int R)
{
    float s2 = max(sigma * sigma, 0.01f);
    float3 acc = 0.0f;
    float sum = 0.0f;
    [loop] for (int dx = -16; dx <= 16; ++dx) {
        if (dx < -R || dx > R) continue;
        float wgt = exp(-0.5f * (float)(dx * dx) / s2);
        acc += liquidGlowMasked(int2(x + dx, y), w, h) * wgt;
        sum += wgt;
    }
    return acc / max(sum, 0.0001f);
}

[numthreads(8,8,1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint w, h;
    g_OutputTexture.GetDimensions(w, h);
    if (dtid.x >= w || dtid.y >= h) return;
    float sigma = clamp(g_P1, 0.1f, 8.0f);
    int R = clamp((int)ceil(sigma * 2.0f), 1, 16);
    float scale = max(g_P3, 4.0f);
    float nx = (float)dtid.x / scale;
    float ny = (float)dtid.y / scale;
    float flowX = sin(ny * 1.73f + g_P5) + sin((nx + ny) * 0.71f - g_P5 * 0.63f) * 0.5f;
    float flowY = cos(nx * 1.37f - g_P5 * 0.81f) + cos((nx - ny) * 0.83f + g_P5) * 0.5f;
    float sx = (float)dtid.x + flowX * g_P4;
    float sy = (float)dtid.y + flowY * g_P4;
    int x0 = (int)floor(sx);
    int y0 = (int)floor(sy);
    float tx = sx - (float)x0;
    float3 v0 = 0.0f;
    float3 v1 = 0.0f;
    float sum = 0.0f;
    [loop] for (int dy = -16; dy <= 16; ++dy) {
        if (dy < -R || dy > R) continue;
        float wgt = exp(-0.5f * (float)(dy * dy) / max(sigma * sigma, 0.01f));
        v0 += liquidGlowHBlur(x0, y0 + dy, w, h, sigma, R) * wgt;
        v1 += liquidGlowHBlur(x0 + 1, y0 + dy, w, h, sigma, R) * wgt;
        sum += wgt;
    }
    float3 flowed = (v0 * (1.0f - tx) + v1 * tx) / max(sum, 0.0001f);
    float4 src = g_InputTexture[dtid.xy];
    float3 result = min(src.rgb + flowed * g_P2, 1.0f);
    g_OutputTexture[dtid.xy] = float4(result, src.a);
}
)";

LiquidGlowEffect::LiquidGlowEffect()
{
    setEffectID(UniString("liquid_glow"));
    setDisplayName(UniString("Liquid Glow"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<LiquidGlowEffectCPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(
        LiquidGlowEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kLiquidGlowResidentHlsl, "main", GpuGenericResourceKind::Filter});
    syncImpls();
}

LiquidGlowEffect::~LiquidGlowEffect() = default;

void LiquidGlowEffect::setThreshold(float value)
{
    threshold_ = std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.55f;
    syncImpls();
}

void LiquidGlowEffect::setRadius(float value)
{
    radius_ = std::isfinite(value) ? std::clamp(value, 0.5f, 64.0f) : 16.0f;
    syncImpls();
}

void LiquidGlowEffect::setIntensity(float value)
{
    intensity_ = std::isfinite(value) ? std::clamp(value, 0.0f, 4.0f) : 1.1f;
    syncImpls();
}

void LiquidGlowEffect::setFlowScale(float value)
{
    flowScale_ = std::isfinite(value) ? std::clamp(value, 4.0f, 256.0f) : 42.0f;
    syncImpls();
}

void LiquidGlowEffect::setDistortion(float value)
{
    distortion_ = std::isfinite(value) ? std::clamp(value, 0.0f, 64.0f) : 8.0f;
    syncImpls();
}

void LiquidGlowEffect::setPhase(float value)
{
    phase_ = std::isfinite(value) ? value : 0.0f;
    syncImpls();
}

void LiquidGlowEffect::syncImpls()
{
    if (auto impl = ArtifactCore::dynamicPointerCast<LiquidGlowEffectCPUImpl>(cpuImpl())) {
        impl->threshold = threshold_;
        impl->radius = radius_;
        impl->intensity = intensity_;
        impl->flowScale = flowScale_;
        impl->distortion = distortion_;
        impl->phase = phase_;
    }
}

std::vector<AbstractProperty> LiquidGlowEffect::getProperties() const
{
    std::vector<AbstractProperty> properties;
    auto& threshold = properties.emplace_back();
    threshold.setName("Threshold");
    threshold.setType(PropertyType::Float);
    threshold.setValue(threshold_);
    auto& radius = properties.emplace_back();
    radius.setName("Radius");
    radius.setType(PropertyType::Float);
    radius.setValue(radius_);
    auto& intensity = properties.emplace_back();
    intensity.setName("Intensity");
    intensity.setType(PropertyType::Float);
    intensity.setValue(intensity_);
    auto& flowScale = properties.emplace_back();
    flowScale.setName("Flow Scale");
    flowScale.setType(PropertyType::Float);
    flowScale.setValue(flowScale_);
    auto& distortion = properties.emplace_back();
    distortion.setName("Distortion");
    distortion.setType(PropertyType::Float);
    distortion.setValue(distortion_);
    auto& phase = properties.emplace_back();
    phase.setName("Phase");
    phase.setType(PropertyType::Float);
    phase.setValue(phase_);
    return properties;
}

void LiquidGlowEffect::setPropertyValue(const UniString& name, const QVariant& value)
{
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) setThreshold(value.toFloat());
    else if (key == QStringLiteral("Radius")) setRadius(value.toFloat());
    else if (key == QStringLiteral("Intensity")) setIntensity(value.toFloat());
    else if (key == QStringLiteral("Flow Scale")) setFlowScale(value.toFloat());
    else if (key == QStringLiteral("Distortion")) setDistortion(value.toFloat());
    else if (key == QStringLiteral("Phase")) setPhase(value.toFloat());
}

} // namespace Artifact
