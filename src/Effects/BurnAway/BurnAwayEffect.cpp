module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QColor>
#include <QString>
#include <QVariant>

module Artifact.Effect.BurnAway;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Graphics.SurfaceColorContract;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;
import Memory.SharedPtr;

namespace Artifact {

using namespace ArtifactCore;

namespace {

// The hash and the value noise below are byte-for-byte the same construction
// as the one used by the turbulent displace effect, so a burn edge and a
// displacement edge share one noise vocabulary across CPU and GPU.
static inline float hash2D(int x, int y, int seed)
{
    std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u
                    + static_cast<std::uint32_t>(y) * 668265263u
                    + static_cast<std::uint32_t>(seed) * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return static_cast<float>(h) / static_cast<float>(0xFFFFFFFFu);
}

static float valueNoise2D(float x, float y, int seed)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);

    const float v00 = hash2D(x0,     y0,     seed);
    const float v10 = hash2D(x0 + 1, y0,     seed);
    const float v01 = hash2D(x0,     y0 + 1, seed);
    const float v11 = hash2D(x0 + 1, y0 + 1, seed);

    const float sx = fx * fx * (3.0f - 2.0f * fx);
    const float sy = fy * fy * (3.0f - 2.0f * fy);
    const float ix0 = v00 + (v10 - v00) * sx;
    const float ix1 = v01 + (v11 - v01) * sx;
    return ix0 + (ix1 - ix0) * sy;
}

static float burnFbm(float x, float y, int octaves, int seed)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    float frequency = 1.0f;
    float norm = 0.0f;
    const int count = std::clamp(octaves, 1, 8);
    for (int octave = 0; octave < count; ++octave) {
        sum += valueNoise2D(x * frequency, y * frequency, seed + octave * 1013) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }
    return norm > 0.0f ? sum / norm : 0.5f;
}

static inline float saturate(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

static inline float smoothstep01(float edge0, float edge1, float value)
{
    if (edge1 <= edge0) {
        return value < edge0 ? 0.0f : 1.0f;
    }
    const float t = saturate((value - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

// Shared front geometry.  At progress 0 the front rests just below the
// layer, so the neutral setting is an exact identity; at progress 1 the front
// travels past the far edge, so every pixel is consumed.
struct BurnFront {
    float distance = 0.0f;  // > 0 keeps the pixel, < 0 burns it away.
};

static BurnFront evaluateBurnFront(float axis, float noise, float progress,
                                    float spread, float softness)
{
    BurnFront front;
    const float frontPosition =
        progress * (1.0f + 0.5f * spread + 2.0f * softness) - softness;
    front.distance = axis - frontPosition - (noise - 0.5f) * spread * progress;
    return front;
}

} // namespace

class BurnAwayEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float progress_ = 0.0f;
    float direction_ = 0.0f;
    float edgeWidth_ = 0.05f;
    float charAmount_ = 0.85f;
    float emberIntensity_ = 0.8f;
    float emberWidth_ = 0.012f;
    float emberRed_ = 1.0f;
    float emberGreen_ = 0.55f;
    float emberBlue_ = 0.16f;
    float edgeSoftness_ = 0.004f;
    float noiseScale_ = 0.06f;
    int octaves_ = 4;
    int seed_ = 0;
    float spread_ = 0.35f;
    float flicker_ = 0.25f;
    float residue_ = 0.0f;
    float sparkDensity_ = 0.5f;
    float timeSeconds_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        const auto& srcImage = src.image();
        const float* pixels = srcImage.rgba32fData();
        if (!pixels) {
            dst = src;
            return;
        }
        const int width = srcImage.width();
        const int height = srcImage.height();
        if (width <= 0 || height <= 0) {
            dst = src;
            return;
        }
        dst = src;

        cv::Mat dstMat(dst.image().height(), dst.image().width(), CV_32FC4,
                       dst.image().rgba32fData());
        const float invHeight = 1.0f / static_cast<float>(height);
        const float noiseExtent = std::max(1e-4f, noiseScale_);
        const float softness = std::max(1e-5f, edgeSoftness_);
        const float charWidth = std::max(1e-5f, edgeWidth_);
        const float emberWidth = std::max(1e-5f, emberWidth_);
        const bool burnFromTop = direction_ >= 0.5f;
        const float activity = smoothstep01(0.0f, 0.05f, progress_);
        // The backing storage order comes from the surface descriptor and
        // varies with the source (QImage sourced surfaces are BGRA, decoded
        // images are RGBA), so the channel writes follow the descriptor
        // instead of assuming one layout.
        const bool redBlueSwapped =
            dst.image().colorDescriptor().channelOrder != SurfaceChannelOrder::RGBA;

        ArtifactCore::Parallel::ForPixels(0, height, width, height, [&](int y) {
            cv::Vec4f* row = dstMat.ptr<cv::Vec4f>(y);
            const float axisTop = (static_cast<float>(y) + 0.5f) * invHeight;
            const float axis = burnFromTop ? axisTop : (1.0f - axisTop);
            for (int x = 0; x < width; ++x) {
                cv::Vec4f& pixel = row[x];
                const float noise = burnFbm(
                    static_cast<float>(x) / (noiseExtent * static_cast<float>(height)),
                    static_cast<float>(y) / (noiseExtent * static_cast<float>(height)),
                    octaves_, seed_);
                const BurnFront front = evaluateBurnFront(axis, noise, progress_, spread_,
                                                           softness);
                const float distance = front.distance;

                const float alphaFactor = smoothstep01(-softness, softness, distance);
                // Nothing chars and nothing glows before the burn starts, so
                // the neutral setting stays an exact identity.
                const float charMask = distance > 0.0f
                    ? saturate(1.0f - distance / charWidth)
                    : 0.0f;
                // A wider, softer band around the char edge reads as soot that
                // has already cooled down.
                const float sootMask = distance > 0.0f
                    ? saturate(1.0f - distance / (charWidth * 3.0f))
                    : 0.0f;
                const float charWeight = charMask * charAmount_ * activity;
                const float sootWeight = sootMask * charAmount_ * 0.45f * activity;
                const float darken = saturate(charWeight + sootWeight);

                const float emberMask = std::exp(-(distance * distance) /
                                                 (emberWidth * emberWidth));
                const float flickerPhase = 0.5f - 0.5f *
                    std::sin(timeSeconds_ * 6.0f + noise * 25.0f);
                const float emberScale = 1.0f - flicker_ * flickerPhase;
                // The ember rides the surviving edge.  The surface is straight
                // alpha, so the final alpha already attenuates the glow where
                // the paper is gone; no extra alpha floor is needed.
                const float emberGain = emberMask * emberIntensity_ * emberScale * activity;

                // Isolated spark points break the smooth rim into embers.
                const float sparkHash = hash2D(x, y, seed_ * 131 + 17);
                const float spark = sparkHash > (1.0f - sparkDensity_ * 0.10f) ? 1.0f : 0.0f;
                const float sparkGain = spark * emberMask * emberIntensity_ * 2.2f *
                                        activity * emberScale;
                // Sparks burn hotter than the rim they sit on, so they read as
                // white-hot points instead of more of the same orange.
                const float sparkRed = 1.0f;
                const float sparkGreen = emberGreen_ + (1.0f - emberGreen_) * 0.55f;
                const float sparkBlue = emberBlue_ + (1.0f - emberBlue_) * 0.55f;

                float alpha = pixel[3] * alphaFactor;

                // The burnt region can keep a faint ash film.  The surface is
                // straight alpha, so the film needs its own coverage.
                const float burntMask = saturate(-distance / softness);
                const float residueAmount = residue_ * burntMask * activity;
                if (residueAmount > 0.0f) {
                    alpha = std::max(alpha, residueAmount * 0.45f);
                }

                // Charred paper keeps a warm dark tint instead of a flat
                // black, and the ember rim is added after the char so the
                // glowing edge stays readable over the soot.
                const float charRed = 1.0f - 0.84f * darken;
                const float charGreen = 1.0f - 0.90f * darken;
                const float charBlue = 1.0f - 0.94f * darken;

                // Read the source channels through the surface order.
                const float sourceRed = redBlueSwapped ? pixel[2] : pixel[0];
                const float sourceGreen = pixel[1];
                const float sourceBlue = redBlueSwapped ? pixel[0] : pixel[2];

                float red = sourceRed * charRed;
                float green = sourceGreen * charGreen;
                float blue = sourceBlue * charBlue;

                if (residueAmount > 0.0f) {
                    const float ash = residueAmount;
                    red = red * (1.0f - ash) + 0.42f * ash;
                    green = green * (1.0f - ash) + 0.40f * ash;
                    blue = blue * (1.0f - ash) + 0.38f * ash;
                }

                const float glow = (emberGain + sparkGain) * alpha;
                red += emberRed_ * emberGain * alpha + sparkRed * sparkGain * alpha;
                green += emberGreen_ * emberGain * alpha + sparkGreen * sparkGain * alpha;
                blue += emberBlue_ * emberGain * alpha + sparkBlue * sparkGain * alpha;

                // The surface buffer stores the channels in the order the
                // surface descriptor declares, so the writes follow it.
                if (redBlueSwapped) {
                    pixel[0] = blue;
                    pixel[1] = green;
                    pixel[2] = red;
                } else {
                    pixel[0] = red;
                    pixel[1] = green;
                    pixel[2] = blue;
                }
                pixel[3] = alpha;
            }
        });
    }
};

class BurnAwayEffectGPUImpl : public ArtifactEffectImplBase {
public:
    BurnAwayEffectCPUImpl cpuImpl_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        cpuImpl_.applyCPU(src, dst);
    }
    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

// Resident-path shader.  The shared generic parameter buffer carries
// P0 progress, P1 direction, P2 edge width, P3 char amount, P4 ember
// intensity, P5 the packed ember colour, P6 ember width, P7 edge softness,
// P8 noise scale, P9 octaves, P10 seed, P11 spread, P12 flicker, P13 the
// composition clock, P14 residue and P15 spark density.  The renderer
// guarantees the parameter buffer at b0; the body only declares the two
// texture views it uses.
static constexpr const char* kBurnAwayResidentHlsl = R"(
Texture2D<float4> g_InputTexture : register(t0);
RWTexture2D<float4> g_OutputTexture : register(u0);

uint burnHash(uint x, uint y, uint seed)
{
    uint h = x * 374761393u + y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= (h >> 16u);
    return h;
}

float burnValueNoise(float2 position, uint seed)
{
    int2 base = int2(floor(position));
    float2 fraction = frac(position);
    uint2 p00 = uint2(base);
    const float v00 = (float)burnHash(p00.x,     p00.y,     seed) / 4294967295.0;
    const float v10 = (float)burnHash(p00.x + 1, p00.y,     seed) / 4294967295.0;
    const float v01 = (float)burnHash(p00.x,     p00.y + 1, seed) / 4294967295.0;
    const float v11 = (float)burnHash(p00.x + 1, p00.y + 1, seed) / 4294967295.0;
    const float2 smooth = fraction * fraction * (3.0 - 2.0 * fraction);
    return lerp(lerp(v00, v10, smooth.x), lerp(v01, v11, smooth.x), smooth.y);
}

float burnFbm(float2 position, int octaves, uint seed)
{
    float sum = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float norm = 0.0;
    const int count = clamp(octaves, 1, 8);
    for (int octave = 0; octave < count; ++octave)
    {
        sum += burnValueNoise(position * frequency, seed + (uint)octave * 1013u) * amplitude;
        norm += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return norm > 0.0 ? sum / norm : 0.5;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (dispatchId.x >= width || dispatchId.y >= height) return;

    const float progress = g_P0;
    const float direction = g_P1;
    const float edgeWidth = max(g_P2, 1e-5);
    const float charAmount = g_P3;
    const float emberIntensity = g_P4;
    // The ember colour travels packed as 8 bits per channel.
    const float packedColor = g_P5;
    const float3 emberColor = float3(
        floor(packedColor / 65536.0),
        floor(fmod(packedColor, 65536.0) / 256.0),
        fmod(packedColor, 256.0)) / 255.0;
    const float emberWidth = max(g_P6, 1e-5);
    const float edgeSoftness = max(g_P7, 1e-5);
    const float noiseScale = max(g_P8, 1e-4);
    const int octaves = clamp((int)(g_P9 + 0.5), 1, 8);
    const uint seed = (uint)max(g_P10, 0.0);
    const float spread = g_P11;
    const float flicker = g_P12;
    const float clock = g_P13;
    const float residue = g_P14;
    const float sparkDensity = g_P15;

    const float invHeight = 1.0 / max((float)height, 1.0);
    const float axisTop = ((float)dispatchId.y + 0.5) * invHeight;
    const float axis = direction >= 0.5 ? axisTop : (1.0 - axisTop);
    const float2 noisePosition = float2((float)dispatchId.x, (float)dispatchId.y) /
                                 (noiseScale * max((float)height, 1.0));
    const float noise = burnFbm(noisePosition, octaves, seed);

    const float frontPosition =
        progress * (1.0 + 0.5 * spread + 2.0 * edgeSoftness) - edgeSoftness;
    const float distance = axis - frontPosition - (noise - 0.5) * spread * progress;

    const float alphaFactor = smoothstep(-edgeSoftness, edgeSoftness, distance);
    // Nothing chars and nothing glows before the burn starts, so the neutral
    // setting stays an exact identity.
    const float activity = smoothstep(0.0, 0.05, progress);
    const float charMask = distance > 0.0 ? saturate(1.0 - distance / edgeWidth) : 0.0;
    const float sootMask = distance > 0.0 ? saturate(1.0 - distance / (edgeWidth * 3.0)) : 0.0;
    const float charWeight = charMask * charAmount * activity;
    const float sootWeight = sootMask * charAmount * 0.45 * activity;
    const float darken = saturate(charWeight + sootWeight);

    const float emberMask = exp(-(distance * distance) / (emberWidth * emberWidth));
    const float flickerPhase = 0.5 - 0.5 * sin(clock * 6.0 + noise * 25.0);
    const float emberScale = 1.0 - flicker * flickerPhase;
    // The ember rides the surviving edge.  The surface is straight alpha, so
    // the final alpha already attenuates the glow where the paper is gone.
    const float emberGain = emberMask * emberIntensity * emberScale * activity;

    // Isolated spark points break the smooth rim into embers.
    const float sparkHash = (float)burnHash(dispatchId.x, dispatchId.y,
                                            seed * 131u + 17u) / 4294967295.0;
    const float spark = sparkHash > (1.0 - sparkDensity * 0.10) ? 1.0 : 0.0;
    const float sparkGain = spark * emberMask * emberIntensity * 2.2 * activity * emberScale;
    // Sparks burn hotter than the rim they sit on, so they read as white-hot
    // points instead of more of the same orange.
    const float3 sparkColor = float3(
        1.0,
        emberColor.g + (1.0 - emberColor.g) * 0.55,
        emberColor.b + (1.0 - emberColor.b) * 0.55);

    float alpha = saturate((g_InputTexture[dispatchId.xy]).a * alphaFactor);

    // The burnt region can keep a faint ash film.
    const float burntMask = saturate(-distance / edgeSoftness);
    const float residueAmount = residue * burntMask * activity;
    if (residueAmount > 0.0)
    {
        alpha = max(alpha, residueAmount * 0.45);
    }

    // The surface buffer stores B,G,R,A, so the texture is swizzled on the
    // way in and on the way out (same convention as the linear wipe shader).
    float4 stored = (g_InputTexture[dispatchId.xy]).bgra;
    float3 charTint = float3(
        1.0 - 0.84 * darken,
        1.0 - 0.90 * darken,
        1.0 - 0.94 * darken);
    float3 colour = stored.xyz * charTint;
    if (residueAmount > 0.0)
    {
        const float ash = residueAmount;
        colour = colour * (1.0 - ash) + float3(0.42, 0.40, 0.38) * ash;
    }
    const float glow = (emberGain + sparkGain) * alpha;
    colour += emberColor * (emberGain * alpha) + sparkColor * (sparkGain * alpha);
    stored.x = colour.b;
    stored.y = colour.g;
    stored.z = colour.r;
    stored.w = alpha;
    g_OutputTexture[dispatchId.xy] = (stored).bgra;
}
)";

BurnAwayEffect::BurnAwayEffect() {
    setDisplayName(UniString("Burn Away"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<BurnAwayEffectCPUImpl>());
    setGPUImpl(ArtifactCore::makeShared<BurnAwayEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    registerGpuGenericShader(
        BurnAwayEffect::kGpuGenericKey,
        GpuGenericShaderRecord{
            kBurnAwayResidentHlsl, "main", GpuGenericResourceKind::Filter});
}
BurnAwayEffect::~BurnAwayEffect() = default;

float BurnAwayEffect::progress() const { return progress_; }
void BurnAwayEffect::setProgress(float v) {
    progress_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    syncImpls();
}
float BurnAwayEffect::direction() const { return direction_; }
void BurnAwayEffect::setDirection(float v) {
    direction_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    syncImpls();
}
float BurnAwayEffect::edgeWidth() const { return edgeWidth_; }
void BurnAwayEffect::setEdgeWidth(float v) {
    edgeWidth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.5f) : 0.05f;
    syncImpls();
}
float BurnAwayEffect::charAmount() const { return charAmount_; }
void BurnAwayEffect::setCharAmount(float v) {
    charAmount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.85f;
    syncImpls();
}
float BurnAwayEffect::emberIntensity() const { return emberIntensity_; }
void BurnAwayEffect::setEmberIntensity(float v) {
    emberIntensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 8.0f) : 0.8f;
    syncImpls();
}
float BurnAwayEffect::emberWidth() const { return emberWidth_; }
void BurnAwayEffect::setEmberWidth(float v) {
    emberWidth_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.5f) : 0.012f;
    syncImpls();
}
QColor BurnAwayEffect::emberColor() const { return emberColor_; }
void BurnAwayEffect::setEmberColor(const QColor& value) {
    emberColor_ = value.isValid() ? value : QColor(255, 140, 40);
    syncImpls();
}
float BurnAwayEffect::edgeSoftness() const { return edgeSoftness_; }
void BurnAwayEffect::setEdgeSoftness(float v) {
    edgeSoftness_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.2f) : 0.004f;
    syncImpls();
}
float BurnAwayEffect::noiseScale() const { return noiseScale_; }
void BurnAwayEffect::setNoiseScale(float v) {
    noiseScale_ = std::isfinite(v) ? std::clamp(v, 0.001f, 1.0f) : 0.06f;
    syncImpls();
}
int BurnAwayEffect::octaves() const { return octaves_; }
void BurnAwayEffect::setOctaves(int v) {
    octaves_ = std::clamp(v, 1, 8);
    syncImpls();
}
int BurnAwayEffect::seed() const { return seed_; }
void BurnAwayEffect::setSeed(int v) {
    seed_ = std::clamp(v, 0, 9999);
    syncImpls();
}
float BurnAwayEffect::spread() const { return spread_; }
void BurnAwayEffect::setSpread(float v) {
    spread_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.35f;
    syncImpls();
}
float BurnAwayEffect::flicker() const { return flicker_; }
void BurnAwayEffect::setFlicker(float v) {
    flicker_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.25f;
    syncImpls();
}
float BurnAwayEffect::residue() const { return residue_; }
void BurnAwayEffect::setResidue(float v) {
    residue_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.0f;
    syncImpls();
}
float BurnAwayEffect::sparkDensity() const { return sparkDensity_; }
void BurnAwayEffect::setSparkDensity(float v) {
    sparkDensity_ = std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 0.5f;
    syncImpls();
}
float BurnAwayEffect::packedEmberColor() const {
    const auto channel = [](float value) {
        return static_cast<float>(std::clamp(
            static_cast<int>(std::lround(value * 255.0f)), 0, 255));
    };
    return channel(emberColor_.redF()) * 65536.0f +
           channel(emberColor_.greenF()) * 256.0f +
           channel(emberColor_.blueF());
}
float BurnAwayEffect::timeSeconds() const { return timeSeconds_; }

void BurnAwayEffect::onContextUpdated(const EffectContext& context) {
    const float seconds = static_cast<float>(context.timeSeconds);
    if (std::isfinite(seconds) && seconds != timeSeconds_) {
        timeSeconds_ = seconds;
        syncImpls();
    }
}

void BurnAwayEffect::syncImpls() {
    if (auto* c = dynamic_cast<BurnAwayEffectCPUImpl*>(cpuImpl().get())) {
        c->progress_ = progress_;
        c->direction_ = direction_;
        c->edgeWidth_ = edgeWidth_;
        c->charAmount_ = charAmount_;
        c->emberIntensity_ = emberIntensity_;
        c->emberWidth_ = emberWidth_;
        c->emberRed_ = emberColor_.redF();
        c->emberGreen_ = emberColor_.greenF();
        c->emberBlue_ = emberColor_.blueF();
        c->edgeSoftness_ = edgeSoftness_;
        c->noiseScale_ = noiseScale_;
        c->octaves_ = octaves_;
        c->seed_ = seed_;
        c->spread_ = spread_;
        c->flicker_ = flicker_;
        c->residue_ = residue_;
        c->sparkDensity_ = sparkDensity_;
        c->timeSeconds_ = timeSeconds_;
    }
    if (auto* g = dynamic_cast<BurnAwayEffectGPUImpl*>(gpuImpl().get())) {
        g->cpuImpl_.progress_ = progress_;
        g->cpuImpl_.direction_ = direction_;
        g->cpuImpl_.edgeWidth_ = edgeWidth_;
        g->cpuImpl_.charAmount_ = charAmount_;
        g->cpuImpl_.emberIntensity_ = emberIntensity_;
        g->cpuImpl_.emberWidth_ = emberWidth_;
        g->cpuImpl_.emberRed_ = emberColor_.redF();
        g->cpuImpl_.emberGreen_ = emberColor_.greenF();
        g->cpuImpl_.emberBlue_ = emberColor_.blueF();
        g->cpuImpl_.edgeSoftness_ = edgeSoftness_;
        g->cpuImpl_.noiseScale_ = noiseScale_;
        g->cpuImpl_.octaves_ = octaves_;
        g->cpuImpl_.seed_ = seed_;
        g->cpuImpl_.spread_ = spread_;
        g->cpuImpl_.flicker_ = flicker_;
        g->cpuImpl_.residue_ = residue_;
        g->cpuImpl_.sparkDensity_ = sparkDensity_;
        g->cpuImpl_.timeSeconds_ = timeSeconds_;
    }
}

std::vector<AbstractProperty> BurnAwayEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(13);

    { auto& p = props.emplace_back(); p.setName("Progress"); p.setType(PropertyType::Float); p.setValue(progress_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Direction"); p.setType(PropertyType::Float); p.setValue(direction_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Edge Width"); p.setType(PropertyType::Float); p.setValue(edgeWidth_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(0.5)); }
    { auto& p = props.emplace_back(); p.setName("Char Amount"); p.setType(PropertyType::Float); p.setValue(charAmount_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Ember Intensity"); p.setType(PropertyType::Float); p.setValue(emberIntensity_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(8.0)); }
    { auto& p = props.emplace_back(); p.setName("Ember Width"); p.setType(PropertyType::Float); p.setValue(emberWidth_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(0.5)); }
    { auto& p = props.emplace_back(); p.setName("Ember Color"); p.setType(PropertyType::Color); p.setValue(emberColor_); p.setDefaultValue(QColor(255, 140, 40)); }
    { auto& p = props.emplace_back(); p.setName("Edge Softness"); p.setType(PropertyType::Float); p.setValue(edgeSoftness_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(0.2)); }
    { auto& p = props.emplace_back(); p.setName("Noise Scale"); p.setType(PropertyType::Float); p.setValue(noiseScale_); p.setMinValue(QVariant(0.001)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Octaves"); p.setType(PropertyType::Integer); p.setValue(octaves_); p.setMinValue(QVariant(1)); p.setMaxValue(QVariant(8)); }
    { auto& p = props.emplace_back(); p.setName("Seed"); p.setType(PropertyType::Integer); p.setValue(seed_); p.setMinValue(QVariant(0)); p.setMaxValue(QVariant(9999)); }
    { auto& p = props.emplace_back(); p.setName("Spread"); p.setType(PropertyType::Float); p.setValue(spread_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Flicker"); p.setType(PropertyType::Float); p.setValue(flicker_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Residue"); p.setType(PropertyType::Float); p.setValue(residue_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    { auto& p = props.emplace_back(); p.setName("Spark Density"); p.setType(PropertyType::Float); p.setValue(sparkDensity_); p.setMinValue(QVariant(0.0)); p.setMaxValue(QVariant(1.0)); }
    return props;
}

void BurnAwayEffect::setPropertyValue(const UniString& n, const QVariant& v) {
    const QString key = n.toQString();
    if (key == QStringLiteral("Progress")) setProgress(v.toFloat());
    else if (key == QStringLiteral("Direction")) setDirection(v.toFloat());
    else if (key == QStringLiteral("Edge Width")) setEdgeWidth(v.toFloat());
    else if (key == QStringLiteral("Char Amount")) setCharAmount(v.toFloat());
    else if (key == QStringLiteral("Ember Intensity")) setEmberIntensity(v.toFloat());
    else if (key == QStringLiteral("Ember Width")) setEmberWidth(v.toFloat());
    else if (key == QStringLiteral("Ember Color")) setEmberColor(v.value<QColor>());
    else if (key == QStringLiteral("Edge Softness")) setEdgeSoftness(v.toFloat());
    else if (key == QStringLiteral("Noise Scale")) setNoiseScale(v.toFloat());
    else if (key == QStringLiteral("Octaves")) setOctaves(v.toInt());
    else if (key == QStringLiteral("Seed")) setSeed(v.toInt());
    else if (key == QStringLiteral("Spread")) setSpread(v.toFloat());
    else if (key == QStringLiteral("Flicker")) setFlicker(v.toFloat());
    else if (key == QStringLiteral("Residue")) setResidue(v.toFloat());
    else if (key == QStringLiteral("Spark Density")) setSparkDensity(v.toFloat());
    else setCommonPropertyValue(key, v);
}

} // namespace Artifact
