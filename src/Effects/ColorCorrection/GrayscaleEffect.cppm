module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <QVariant>

module GrayscaleEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {
constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;
constexpr float kLinearR = 0.2126f;
constexpr float kLinearG = 0.7152f;
constexpr float kLinearB = 0.0722f;

float srgbToLinear(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (v <= 0.04045f) {
        return v / 12.92f;
    }
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float v)
{
    v = std::clamp(v, 0.0f, 1.0f);
    if (v <= 0.0031308f) {
        return v * 12.92f;
    }
    return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

const char* kGrayscaleHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);
cbuffer GrayscaleParams : register(b0)
{
    float g_Strength;
    float g_Mode;
    float3 g_Pad;
};

float srgbToLinear(float v)
{
    return (v <= 0.04045f) ? (v / 12.92f) : pow((v + 0.055f) / 1.055f, 2.4f);
}

float linearToSrgb(float v)
{
    return (v <= 0.0031308f) ? (v * 12.92f) : (1.055f * pow(v, 1.0f / 2.4f) - 0.055f);
}

[numthreads(8, 8, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dtid.x >= width || dtid.y >= height) return;

    float4 c = g_InputTexture[dtid.xy];
    float gray = 0.0f;
    if (g_Mode < 0.5f) {
        gray = dot(c.rgb, float3(0.299f, 0.587f, 0.114f));
    } else if (g_Mode < 1.5f) {
        float3 linearRgb = float3(srgbToLinear(c.r), srgbToLinear(c.g), srgbToLinear(c.b));
        float linearGray = dot(linearRgb, float3(0.2126f, 0.7152f, 0.0722f));
        gray = linearToSrgb(linearGray);
    } else {
        gray = (max(c.r, max(c.g, c.b)) + min(c.r, min(c.g, c.b))) * 0.5f;
    }
    c.rgb = lerp(c.rgb, gray.xxx, saturate(g_Strength));
    g_OutputTexture[dtid.xy] = c;
}
)";
} // namespace

class GrayscaleEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    int mode_ = 1;

    float grayscaleForPixel(float r, float g, float b) const {
        switch (mode_) {
        case 0:
            return r * kLumaR + g * kLumaG + b * kLumaB;
        case 1: {
            const float linearR = srgbToLinear(r);
            const float linearG = srgbToLinear(g);
            const float linearB = srgbToLinear(b);
            return linearToSrgb(linearR * kLinearR + linearG * kLinearG + linearB * kLinearB);
        }
        case 2:
        default:
            return (std::max({r, g, b}) + std::min({r, g, b})) * 0.5f;
        }
    }

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        const float strength = std::clamp(strength_, 0.0f, 1.0f);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                const float gray = grayscaleForPixel(pixel[0], pixel[1], pixel[2]);
                pixel[0] = std::lerp(pixel[0], gray, strength);
                pixel[1] = std::lerp(pixel[1], gray, strength);
                pixel[2] = std::lerp(pixel[2], gray, strength);
            }
        }
    }
};

class GrayscaleEffectGPUImpl : public ArtifactEffectImplBase {
public:
    float strength_ = 1.0f;
    int mode_ = 1;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.strength_ = strength_;
        cpuImpl_.mode_ = mode_;
        cpuImpl_.applyCPU(src, dst);
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }

private:
    GrayscaleEffectCPUImpl cpuImpl_;
};

GrayscaleEffect::GrayscaleEffect() {
    setEffectID(UniString("effect.colorcorrection.grayscale"));
    setDisplayName(UniString("Grayscale"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<GrayscaleEffectCPUImpl>());
    setGPUImpl(std::make_shared<GrayscaleEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
}

GrayscaleEffect::~GrayscaleEffect() = default;

void GrayscaleEffect::setStrength(float value) {
    strength_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void GrayscaleEffect::setMode(int value) {
    mode_ = static_cast<Mode>(std::clamp(value, 0, 2));
    syncImpls();
}

void GrayscaleEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<GrayscaleEffectCPUImpl*>(cpuImpl().get())) {
        cpu->strength_ = strength_;
        cpu->mode_ = static_cast<int>(mode_);
    }
    if (auto* gpu = dynamic_cast<GrayscaleEffectGPUImpl*>(gpuImpl().get())) {
        gpu->strength_ = strength_;
        gpu->mode_ = static_cast<int>(mode_);
    }
}

std::vector<AbstractProperty> GrayscaleEffect::getProperties() const {
    std::vector<AbstractProperty> props(2);
    props[0].setName("Mode");
    props[0].setType(PropertyType::Integer);
    props[0].setValue(QVariant(mode()));
    props[1].setName("Strength");
    props[1].setType(PropertyType::Float);
    props[1].setValue(QVariant(static_cast<double>(strength_)));
    return props;
}

void GrayscaleEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    if (name == "Mode") {
        setMode(value.toInt());
    } else if (name == "Strength") {
        setStrength(value.toFloat());
    }
}

} // namespace Artifact
