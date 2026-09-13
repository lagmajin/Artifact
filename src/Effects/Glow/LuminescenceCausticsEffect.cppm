module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>
#include <QVariant>

module Artifact.Effect.Glow.LuminescenceCaustics;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

class LuminescenceCausticsCPUImpl final : public ArtifactEffectImplBase {
public:
    float threshold = 0.55f;
    float edgeWeight = 0.8f;
    float scale = 22.0f;
    float intensity = 0.75f;
    float evolution = 0.0f;
    float colorShift = 0.35f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache& dst) override {        const auto& image = src.image();
        const float* pixels = image.rgba32fData();
        const int width = image.width();
        const int height = image.height();
        if (!pixels || width <= 0 || height <= 0) { dst = src; return; }

        cv::Mat input(height, width, CV_32FC4, const_cast<float*>(pixels));
        cv::Mat rgb, gray, gradX, gradY, edge;
        cv::cvtColor(input, rgb, cv::COLOR_RGBA2RGB);
        cv::cvtColor(rgb, gray, cv::COLOR_RGB2GRAY);
        cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
        cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
        cv::magnitude(gradX, gradY, edge);
        cv::GaussianBlur(edge, edge, cv::Size(), 1.2, 1.2);

        cv::Mat output = input.clone();
        const float phase = evolution * 0.0174532925f;
        const float invScale = 1.0f / std::max(scale, 1.0f);
        ArtifactCore::Parallel::For(0, height, width * height, [&](int y) {
            const auto* sourceRow = input.ptr<cv::Vec4f>(y);
            const float* grayRow = gray.ptr<float>(y);
            const float* edgeRow = edge.ptr<float>(y);
            const float* gxRow = gradX.ptr<float>(y);
            const float* gyRow = gradY.ptr<float>(y);
            auto* outputRow = output.ptr<cv::Vec4f>(y);
            for (int x = 0; x < width; ++x) {
                const float highlight = std::clamp(
                    (grayRow[x] - threshold) / std::max(0.001f, 1.0f - threshold),
                    0.0f, 1.0f);
                const float direction = std::atan2(gyRow[x], gxRow[x]);
                const float px = x * invScale;
                const float py = y * invScale;
                const float interference = std::abs(
                    std::sin(px * 1.73f + py * 1.17f + phase + direction) +
                    std::sin(px * -1.11f + py * 2.03f - phase * 0.73f));
                const float ridge = std::pow(std::clamp(interference * 0.5f, 0.0f, 1.0f), 5.0f);
                const float sourceMask = std::clamp(highlight + edgeRow[x] * edgeWeight, 0.0f, 1.0f);
                const float caustic = ridge * sourceMask * intensity * sourceRow[x][3];
                const float warm = 1.0f - colorShift * 0.25f;
                outputRow[x][0] = sourceRow[x][0] + caustic * warm;
                outputRow[x][1] = sourceRow[x][1] + caustic;
                outputRow[x][2] = sourceRow[x][2] + caustic * (1.0f + colorShift * 0.65f);
                outputRow[x][3] = sourceRow[x][3];
            }
        });
        dst = src;
        dst.image().setFromCVMat(output, src.image().colorDescriptor());
    }
};

// Resident-path HLSL. Mirrors LuminescenceCausticsCPUImpl: BT.601 gray,
// 3x3 Sobel magnitude, separable Gaussian (sigma 1.2, 9-tap), interference
// ridge pattern, masked additive composite with warm shift. Border uses
// reflect101 to match OpenCV BORDER_DEFAULT. No pixel-unit parameters, so
// no resolution scaling is needed.
static constexpr const char* kCausticsResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

int causticsReflect(int v, int limit)
{
    if (limit <= 1) return 0;
    if (v < 0) return -v;
    if (v >= limit) return 2 * limit - v - 2;
    return v;
}

float causticsGray(int2 p, uint w, uint h)
{
    p.x = causticsReflect(p.x, (int)w);
    p.y = causticsReflect(p.y, (int)h);
    float3 c = g_InputTexture[uint2(p)].rgb;
    return dot(c, float3(0.299f, 0.587f, 0.114f));
}

float2 causticsSobel(int x, int y, uint w, uint h)
{
    float tl = causticsGray(int2(x - 1, y - 1), w, h);
    float t = causticsGray(int2(x, y - 1), w, h);
    float tr = causticsGray(int2(x + 1, y - 1), w, h);
    float l = causticsGray(int2(x - 1, y), w, h);
    float r = causticsGray(int2(x + 1, y), w, h);
    float bl = causticsGray(int2(x - 1, y + 1), w, h);
    float b = causticsGray(int2(x, y + 1), w, h);
    float br = causticsGray(int2(x + 1, y + 1), w, h);
    float gx = (tr + 2.0f * r + br) - (tl + 2.0f * l + bl);
    float gy = (bl + 2.0f * b + br) - (tl + 2.0f * t + tr);
    return float2(gx, gy);
}

float causticsEdgeH(int x, int y, uint w, uint h)
{
    float acc = 0.0f;
    float sum = 0.0f;
    [loop] for (int dx = -4; dx <= 4; ++dx) {
        float d = (float)(dx * dx);
        float wgt = exp(-0.5f * d / 1.44f);
        acc += length(causticsSobel(x + dx, y, w, h)) * wgt;
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
    int x = (int)dtid.x;
    int y = (int)dtid.y;
    float acc = 0.0f;
    float sum = 0.0f;
    [loop] for (int dy = -4; dy <= 4; ++dy) {
        float d = (float)(dy * dy);
        float wgt = exp(-0.5f * d / 1.44f);
        acc += causticsEdgeH(x, y + dy, w, h) * wgt;
        sum += wgt;
    }
    float edge = acc / max(sum, 0.0001f);
    float2 grad = causticsSobel(x, y, w, h);
    float gray = causticsGray(int2(x, y), w, h);
    float4 src = g_InputTexture[dtid.xy];
    float highlight = clamp((gray - g_P0) / max(0.001f, 1.0f - g_P0), 0.0f, 1.0f);
    float direction = atan2(grad.y, grad.x);
    float phase = g_P4 * 0.0174532925f;
    float invScale = 1.0f / max(g_P2, 1.0f);
    float px = (float)x * invScale;
    float py = (float)y * invScale;
    float interference = abs(sin(px * 1.73f + py * 1.17f + phase + direction) +
                             sin(px * -1.11f + py * 2.03f - phase * 0.73f));
    float ridge = pow(clamp(interference * 0.5f, 0.0f, 1.0f), 5.0f);
    float sourceMask = clamp(highlight + edge * g_P1, 0.0f, 1.0f);
    float caustic = ridge * sourceMask * g_P3 * src.a;
    float warm = 1.0f - g_P5 * 0.25f;
    float3 result = src.rgb + float3(caustic * warm, caustic, caustic * (1.0f + g_P5 * 0.65f));
    g_OutputTexture[dtid.xy] = float4(result, src.a);
}
)";

LuminescenceCausticsEffect::LuminescenceCausticsEffect() {
    setDisplayName(UniString("Luminescence Caustics"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<LuminescenceCausticsCPUImpl>());
    registerGpuGenericShader(
        LuminescenceCausticsEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kCausticsResidentHlsl, "main", GpuGenericResourceKind::Filter});
    syncImpl();
}

LuminescenceCausticsEffect::~LuminescenceCausticsEffect() = default;

void LuminescenceCausticsEffect::syncImpl() {
    if (auto* impl = dynamic_cast<LuminescenceCausticsCPUImpl*>(cpuImpl().get())) {
        impl->threshold = threshold_; impl->edgeWeight = edgeWeight_;
        impl->scale = scale_; impl->intensity = intensity_;
        impl->evolution = evolution_; impl->colorShift = colorShift_;
    }
}

std::vector<AbstractProperty> LuminescenceCausticsEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    auto& threshold = props.emplace_back(); threshold.setName("Threshold"); threshold.setType(PropertyType::Float); threshold.setValue(threshold_);
    auto& edge = props.emplace_back(); edge.setName("Edge Weight"); edge.setType(PropertyType::Float); edge.setValue(edgeWeight_);
    auto& scale = props.emplace_back(); scale.setName("Scale"); scale.setType(PropertyType::Float); scale.setValue(scale_);
    auto& intensity = props.emplace_back(); intensity.setName("Intensity"); intensity.setType(PropertyType::Float); intensity.setValue(intensity_);
    auto& evolution = props.emplace_back(); evolution.setName("Evolution"); evolution.setType(PropertyType::Float); evolution.setValue(evolution_);
    auto& color = props.emplace_back(); color.setName("Color Shift"); color.setType(PropertyType::Float); color.setValue(colorShift_);
    return props;
}

void LuminescenceCausticsEffect::setPropertyValue(const UniString& name,
                                                  const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Threshold")) { const float v = value.toFloat(); threshold_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.55f; }
    else if (key == QStringLiteral("Edge Weight")) { const float v = value.toFloat(); edgeWeight_ = std::isfinite(v) ? std::clamp(v, 0.0f, 5.0f) : 0.8f; }
    else if (key == QStringLiteral("Scale")) { const float v = value.toFloat(); scale_ = std::isfinite(v) ? std::clamp(v, 2.0f, 200.0f) : 22.0f; }
    else if (key == QStringLiteral("Intensity")) { const float v = value.toFloat(); intensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 5.0f) : 0.75f; }
    else if (key == QStringLiteral("Evolution")) { const float v = value.toFloat(); evolution_ = std::isfinite(v) ? v : 0.0f; }
    else if (key == QStringLiteral("Color Shift")) { const float v = value.toFloat(); colorShift_ = std::isfinite(v) ? std::clamp(v, -1.0f, 1.0f) : 0.35f; }
    syncImpl();
}

}
