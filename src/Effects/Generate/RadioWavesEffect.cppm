module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include <QColor>
#include <QString>
#include <QVariant>
#include <opencv2/opencv.hpp>

module Artifact.Effect.Generate.RadioWaves;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

using namespace ArtifactCore;

// ─── CPU Impl ────────────────────────────────────────────────────────────────

class RadioWavesCPUImpl : public ArtifactEffectImplBase {
public:
    float  originX_     = 0.5f;
    float  originY_     = 0.5f;
    float  frequency_   = 2.0f;
    float  expansion_   = 80.0f;
    float  lifespan_    = 2.0f;
    float  strokeWidth_ = 3.0f;
    float  opacity_     = 80.0f;
    QColor waveColor_   = QColor(100, 200, 255, 220);
    float  currentTime_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&       dst) override
    {
        const ImageF32x4_RGBA& srcImg = src.image();
        const float* srcData = srcImg.rgba32fData();
        if (!srcData || srcImg.width() <= 0 || srcImg.height() <= 0) {
            dst = src;
            return;
        }

        const int W = srcImg.width();
        const int H = srcImg.height();

        // ソースをコピー
        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();

        const float cx = originX_ * static_cast<float>(W);
        const float cy = originY_ * static_cast<float>(H);
        const float maxRadius = std::sqrt(static_cast<float>(W * W + H * H));

        // 波色 (float)
        const float wr = waveColor_.redF();
        const float wg = waveColor_.greenF();
        const float wb = waveColor_.blueF();
        const float wa = waveColor_.alphaF() * std::clamp(opacity_ / 100.0f, 0.0f, 1.0f);

        const float halfStroke = strokeWidth_ * 0.5f;

        // 現在時刻に存在する全ウェーブを列挙
        // 各ウェーブは lifespan_ 秒間生存し、frequency_ [波/秒] で発生する
        const int waveCount = static_cast<int>(std::ceil(currentTime_ * frequency_)) + 1;

        for (int y = 0; y < H; ++y) {
            float* row = dstData + y * W * 4;
            const float dy = static_cast<float>(y) - cy;
            for (int x = 0; x < W; ++x) {
                const float dx = static_cast<float>(x) - cx;
                const float dist = std::sqrt(dx * dx + dy * dy);

                float blendAlpha = 0.0f;

                // 各ウェーブを合成（最大 alphaを取る）
                for (int i = 0; i < waveCount; ++i) {
                    // ウェーブ誕生時刻
                    const float birthTime = static_cast<float>(i) / frequency_;
                    const float age = currentTime_ - birthTime;
                    if (age < 0.0f || age > lifespan_) continue;

                    // 現在のウェーブ半径
                    const float radius = age * expansion_;
                    if (radius < 0.0f || radius > maxRadius + halfStroke) continue;

                    // 距離 vs ウェーブリング
                    const float diff = std::abs(dist - radius);
                    if (diff > halfStroke + 1.0f) continue;

                    // リングの輝度（アンチエイリアス + フェード）
                    const float edgeFade = std::clamp(1.0f - (diff - halfStroke), 0.0f, 1.0f);
                    const float lifeFade = 1.0f - std::clamp(age / lifespan_, 0.0f, 1.0f);
                    blendAlpha = std::max(blendAlpha, edgeFade * lifeFade);
                }

                if (blendAlpha < 1e-5f) continue;

                // ポーター・ダフ src over
                float* px = row + x * 4;
                const float srcA = wa * blendAlpha;
                const float dstA = px[3];
                const float outA = srcA + dstA * (1.0f - srcA);
                if (outA < 1e-6f) {
                    px[0] = px[1] = px[2] = px[3] = 0.f;
                } else {
                    px[0] = (wr * srcA + px[0] * dstA * (1.0f - srcA)) / outA;
                    px[1] = (wg * srcA + px[1] * dstA * (1.0f - srcA)) / outA;
                    px[2] = (wb * srcA + px[2] * dstA * (1.0f - srcA)) / outA;
                    px[3] = outA;
                }
            }
        }
    }
};

// ─── RadioWavesEffect ─────────────────────────────────────────────────────────

RadioWavesEffect::RadioWavesEffect()
{
    setDisplayName(UniString("Radio Waves"));
    setPipelineStage(EffectPipelineStage::Rasterizer);

    auto cpu = std::make_shared<RadioWavesCPUImpl>();
    setCPUImpl(cpu);
}

RadioWavesEffect::~RadioWavesEffect() = default;

// ── アクセサ ─────────────────────────────────────────────────────────────────

float RadioWavesEffect::originX() const { return originX_; }
void  RadioWavesEffect::setOriginX(float v) { originX_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

float RadioWavesEffect::originY() const { return originY_; }
void  RadioWavesEffect::setOriginY(float v) { originY_ = std::clamp(v, 0.0f, 1.0f); syncImpls(); }

float RadioWavesEffect::frequency() const { return frequency_; }
void  RadioWavesEffect::setFrequency(float v) { frequency_ = std::max(0.01f, v); syncImpls(); }

float RadioWavesEffect::expansion() const { return expansion_; }
void  RadioWavesEffect::setExpansion(float v) { expansion_ = std::max(0.0f, v); syncImpls(); }

float RadioWavesEffect::lifespan() const { return lifespan_; }
void  RadioWavesEffect::setLifespan(float v) { lifespan_ = std::max(0.01f, v); syncImpls(); }

float RadioWavesEffect::strokeWidth() const { return strokeWidth_; }
void  RadioWavesEffect::setStrokeWidth(float v) { strokeWidth_ = std::max(0.1f, v); syncImpls(); }

float RadioWavesEffect::opacity() const { return opacity_; }
void  RadioWavesEffect::setOpacity(float v) { opacity_ = std::clamp(v, 0.0f, 100.0f); syncImpls(); }

QColor RadioWavesEffect::waveColor() const { return waveColor_; }
void   RadioWavesEffect::setWaveColor(const QColor& c) { waveColor_ = c; syncImpls(); }

float RadioWavesEffect::currentTime() const { return currentTime_; }
void  RadioWavesEffect::setCurrentTime(float t) { currentTime_ = std::max(0.0f, t); syncImpls(); }

// ── Properties API ────────────────────────────────────────────────────────────

std::vector<AbstractProperty> RadioWavesEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(9);

    {auto& p = props.emplace_back(); p.setName("Origin X");    p.setType(PropertyType::Float);   p.setValue(originX_);}
    {auto& p = props.emplace_back(); p.setName("Origin Y");    p.setType(PropertyType::Float);   p.setValue(originY_);}
    {auto& p = props.emplace_back(); p.setName("Frequency");   p.setType(PropertyType::Float);   p.setValue(frequency_);}
    {auto& p = props.emplace_back(); p.setName("Expansion");   p.setType(PropertyType::Float);   p.setValue(expansion_);}
    {auto& p = props.emplace_back(); p.setName("Lifespan");    p.setType(PropertyType::Float);   p.setValue(lifespan_);}
    {auto& p = props.emplace_back(); p.setName("Stroke Width");p.setType(PropertyType::Float);   p.setValue(strokeWidth_);}
    {auto& p = props.emplace_back(); p.setName("Opacity");     p.setType(PropertyType::Float);   p.setValue(opacity_);}
    {auto& p = props.emplace_back(); p.setName("Wave Color");  p.setType(PropertyType::Color);   p.setValue(waveColor_);}
    {auto& p = props.emplace_back(); p.setName("Current Time");p.setType(PropertyType::Float);   p.setValue(currentTime_);}

    return props;
}

void RadioWavesEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString k = name.toQString();
    if      (k == "Origin X")     setOriginX(value.toFloat());
    else if (k == "Origin Y")     setOriginY(value.toFloat());
    else if (k == "Frequency")    setFrequency(value.toFloat());
    else if (k == "Expansion")    setExpansion(value.toFloat());
    else if (k == "Lifespan")     setLifespan(value.toFloat());
    else if (k == "Stroke Width") setStrokeWidth(value.toFloat());
    else if (k == "Opacity")      setOpacity(value.toFloat());
    else if (k == "Wave Color")   setWaveColor(value.value<QColor>());
    else if (k == "Current Time") setCurrentTime(value.toFloat());
}

void RadioWavesEffect::syncImpls() {
    if (auto* c = dynamic_cast<RadioWavesCPUImpl*>(cpuImpl().get())) {
        c->originX_     = originX_;
        c->originY_     = originY_;
        c->frequency_   = frequency_;
        c->expansion_   = expansion_;
        c->lifespan_    = lifespan_;
        c->strokeWidth_ = strokeWidth_;
        c->opacity_     = opacity_;
        c->waveColor_   = waveColor_;
        c->currentTime_ = currentTime_;
    }
}

} // namespace Artifact
