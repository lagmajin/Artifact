module;
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

module ShadowHighlightEffect;
import Memory.SharedPtr;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import Core.Parallel;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageSurfaceView;
import Math.Vec;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

namespace {

constexpr float kLumaR = 0.299f;
constexpr float kLumaG = 0.587f;
constexpr float kLumaB = 0.114f;

inline float clamp01(float v) {
    return std::clamp(v, 0.0f, 1.0f);
}

inline float perceptualWeight(float luma, float center, float width) {
    const float d = (luma - center) / std::max(width, 1e-4f);
    return std::clamp(d * d, 0.0f, 1.0f);
}

inline float luminanceOf(float r, float g, float b) {
    return r * kLumaR + g * kLumaG + b * kLumaB;
}

} // namespace

class ShadowHighlightEffectCPUImpl : public ArtifactEffectImplBase {
public:
    float shadowAmount_ = 50.0f;
    float shadowTonalWidth_ = 50.0f;
    ArtifactCore::Units::Pixels shadowRadius_{30.0f};
    float highlightAmount_ = 0.0f;
    float highlightTonalWidth_ = 50.0f;
    ArtifactCore::Units::Pixels highlightRadius_{30.0f};
    float colorCorrection_ = 20.0f;
    float midtoneContrast_ = 0.0f;
    float blackClip_ = 0.01f;
    float whiteClip_ = 0.01f;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }

        const int width = dst.image().width();
        const int height = dst.image().height();
        if (width <= 0 || height <= 0) {
            return;
        }

        const float shadowAmount = std::clamp(shadowAmount_, 0.0f, 100.0f) / 100.0f;
        const float highlightAmount = std::clamp(highlightAmount_, 0.0f, 100.0f) / 100.0f;
        const float shadowWidth = std::clamp(shadowTonalWidth_, 1.0f, 100.0f) / 100.0f;
        const float highlightWidth = std::clamp(highlightTonalWidth_, 1.0f, 100.0f) / 100.0f;
        const float blackClip = std::clamp(blackClip_, 0.0f, 0.5f);
        const float whiteClip = std::clamp(whiteClip_, 0.0f, 0.5f);
        const float colorCorrection = std::clamp(colorCorrection_, -100.0f, 100.0f) / 100.0f;
        const float midtoneContrast = std::clamp(midtoneContrast_, -100.0f, 100.0f) / 100.0f;

        const float shadowRadius = std::max(shadowRadius_.value, 0.0f);
        const float highlightRadius = std::max(highlightRadius_.value, 0.0f);

        const bool shadowActive = shadowAmount > 0.0f && shadowRadius > 0.0f;
        const bool highlightActive = highlightAmount > 0.0f && highlightRadius > 0.0f;
        const bool hasGlobal = colorCorrection != 0.0f || midtoneContrast != 0.0f;
        const bool hasClip = blackClip > 0.0f || whiteClip > 0.0f;

        if (!shadowActive && !highlightActive && !hasGlobal && !hasClip) {
            return;
        }

        const float blackClipLuma = blackClip;
        const float whiteClipLuma = 1.0f - whiteClip;
        const float clipSpan = std::max(whiteClipLuma - blackClipLuma, 1e-4f);

        ArtifactCore::withMutableColorFloat4View(dst.image().surfaceView(), pixels, [&](auto& view) {
            Parallel::ForPixels(0, height, width, height, [&](int y) {
            auto row = view.row(y);
            for (int x = 0; x < width; ++x) {
                auto pixel = row[x];
                const float r = pixel.r;
                const float g = pixel.g;
                const float b = pixel.b;
                const float luma = luminanceOf(r, g, b);

                float outR = r;
                float outG = g;
                float outB = b;

                // Shadows: 暗部を perceptualWeight で持ち上げる。
                if (shadowActive) {
                    const float w = perceptualWeight(luma, shadowWidth * 0.5f, shadowWidth * 0.5f);
                    if (w > 0.0f) {
                        const float lift = shadowAmount * w * 0.5f;
                        outR += lift;
                        outG += lift;
                        outB += lift;
                    }
                }

                // Highlights: 明部を perceptualWeight で引き下げる。
                if (highlightActive) {
                    const float w = perceptualWeight(luma, 1.0f - highlightWidth * 0.5f, highlightWidth * 0.5f);
                    if (w > 0.0f) {
                        const float pull = highlightAmount * w * 0.5f;
                        outR -= pull;
                        outG -= pull;
                        outB -= pull;
                    }
                }

                // Midtone contrast: 0.5 を pivot として contracts/expand する。
                if (midtoneContrast != 0.0f) {
                    const float pivot = 0.5f;
                    const float factor = 1.0f + midtoneContrast;
                    outR = (outR - pivot) * factor + pivot;
                    outG = (outG - pivot) * factor + pivot;
                    outB = (outB - pivot) * factor + pivot;
                }

                // Color correction: 彩度を colorCorrection だけ調整（0 = 無効）。
                if (colorCorrection != 0.0f) {
                    const float gray = luminanceOf(outR, outG, outB);
                    const float satFactor = 1.0f + colorCorrection;
                    outR = gray + (outR - gray) * satFactor;
                    outG = gray + (outG - gray) * satFactor;
                    outB = gray + (outB - gray) * satFactor;
                }

                // Black / white clip。
                if (hasClip) {
                    outR = (outR - blackClipLuma) / clipSpan;
                    outG = (outG - blackClipLuma) / clipSpan;
                    outB = (outB - blackClipLuma) / clipSpan;
                }

                pixel.r = clamp01(outR);
                pixel.g = clamp01(outG);
                pixel.b = clamp01(outB);
            }
                });
        });
    }
};

ShadowHighlightEffect::ShadowHighlightEffect() {
    setEffectID(UniString("effect.colorcorrection.shadowhighlight"));
    setDisplayName(UniString("Shadow / Highlight"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(ArtifactCore::makeShared<ShadowHighlightEffectCPUImpl>());
    setComputeMode(ComputeMode::CPU);
    syncImpls();
}

ShadowHighlightEffect::~ShadowHighlightEffect() = default;

float ShadowHighlightEffect::shadowAmount() const { return shadowAmount_; }
void ShadowHighlightEffect::setShadowAmount(float v) {
    shadowAmount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 100.0f) : 50.0f;
    syncImpls();
}

float ShadowHighlightEffect::shadowTonalWidth() const { return shadowTonalWidth_; }
void ShadowHighlightEffect::setShadowTonalWidth(float v) {
    shadowTonalWidth_ = std::isfinite(v) ? std::clamp(v, 1.0f, 100.0f) : 50.0f;
    syncImpls();
}

ArtifactCore::Units::Pixels ShadowHighlightEffect::shadowRadius() const { return shadowRadius_; }
void ShadowHighlightEffect::setShadowRadius(ArtifactCore::Units::Pixels v) {
    shadowRadius_ = ArtifactCore::Units::Pixels{
        std::isfinite(v.value) ? std::max(v.value, 0.0f) : 30.0f};
    syncImpls();
}

float ShadowHighlightEffect::highlightAmount() const { return highlightAmount_; }
void ShadowHighlightEffect::setHighlightAmount(float v) {
    highlightAmount_ = std::isfinite(v) ? std::clamp(v, 0.0f, 100.0f) : 0.0f;
    syncImpls();
}

float ShadowHighlightEffect::highlightTonalWidth() const { return highlightTonalWidth_; }
void ShadowHighlightEffect::setHighlightTonalWidth(float v) {
    highlightTonalWidth_ = std::isfinite(v) ? std::clamp(v, 1.0f, 100.0f) : 50.0f;
    syncImpls();
}

ArtifactCore::Units::Pixels ShadowHighlightEffect::highlightRadius() const { return highlightRadius_; }
void ShadowHighlightEffect::setHighlightRadius(ArtifactCore::Units::Pixels v) {
    highlightRadius_ = ArtifactCore::Units::Pixels{
        std::isfinite(v.value) ? std::max(v.value, 0.0f) : 30.0f};
    syncImpls();
}

float ShadowHighlightEffect::colorCorrection() const { return colorCorrection_; }
void ShadowHighlightEffect::setColorCorrection(float v) {
    colorCorrection_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 20.0f;
    syncImpls();
}

float ShadowHighlightEffect::midtoneContrast() const { return midtoneContrast_; }
void ShadowHighlightEffect::setMidtoneContrast(float v) {
    midtoneContrast_ = std::isfinite(v) ? std::clamp(v, -100.0f, 100.0f) : 0.0f;
    syncImpls();
}

float ShadowHighlightEffect::blackClip() const { return blackClip_; }
void ShadowHighlightEffect::setBlackClip(float v) {
    blackClip_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.5f) : 0.01f;
    syncImpls();
}

float ShadowHighlightEffect::whiteClip() const { return whiteClip_; }
void ShadowHighlightEffect::setWhiteClip(float v) {
    whiteClip_ = std::isfinite(v) ? std::clamp(v, 0.0f, 0.5f) : 0.01f;
    syncImpls();
}

void ShadowHighlightEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<ShadowHighlightEffectCPUImpl*>(cpuImpl().get())) {
        cpu->shadowAmount_ = shadowAmount_;
        cpu->shadowTonalWidth_ = shadowTonalWidth_;
        cpu->shadowRadius_ = shadowRadius_;
        cpu->highlightAmount_ = highlightAmount_;
        cpu->highlightTonalWidth_ = highlightTonalWidth_;
        cpu->highlightRadius_ = highlightRadius_;
        cpu->colorCorrection_ = colorCorrection_;
        cpu->midtoneContrast_ = midtoneContrast_;
        cpu->blackClip_ = blackClip_;
        cpu->whiteClip_ = whiteClip_;
    }
}

std::vector<AbstractProperty> ShadowHighlightEffect::getProperties() const {
    std::vector<AbstractProperty> props(10);
    props[0].setName("Shadow Amount");        props[0].setType(PropertyType::Float);
    props[0].setDisplayLabel(QStringLiteral("Shadow Amount"));
    props[0].setValue(QVariant(static_cast<double>(shadowAmount_)));
    props[0].setDefaultValue(50.0);  props[0].setHardRange(0.0, 100.0);
    props[0].setSoftRange(0.0, 75.0); props[0].setStep(0.1);
    props[0].setUnit(QStringLiteral("%"));
    props[0].setTooltip(QStringLiteral("Strength of the shadow lift."));

    props[1].setName("Shadow Tonal Width");   props[1].setType(PropertyType::Float);
    props[1].setDisplayLabel(QStringLiteral("Shadow Tonal Width"));
    props[1].setValue(QVariant(static_cast<double>(shadowTonalWidth_)));
    props[1].setDefaultValue(50.0);  props[1].setHardRange(1.0, 100.0);
    props[1].setSoftRange(5.0, 80.0); props[1].setStep(0.1);
    props[1].setUnit(QStringLiteral("%"));
    props[1].setTooltip(QStringLiteral("Luminance range affected by the shadow adjustment."));

    props[2].setName("Shadow Radius");        props[2].setType(PropertyType::Float);
    props[2].setDisplayLabel(QStringLiteral("Shadow Radius"));
    props[2].setValue(QVariant(static_cast<double>(shadowRadius_.value)));
    props[2].setDefaultValue(30.0);  props[2].setHardRange(0.0, 200.0);
    props[2].setSoftRange(0.0, 60.0); props[2].setStep(0.1);
    props[2].setUnit(QStringLiteral("px"));
    props[2].setTooltip(QStringLiteral("Radius used to separate shadow detail from surrounding tones."));

    props[3].setName("Highlight Amount");     props[3].setType(PropertyType::Float);
    props[3].setDisplayLabel(QStringLiteral("Highlight Amount"));
    props[3].setValue(QVariant(static_cast<double>(highlightAmount_)));
    props[3].setDefaultValue(0.0);   props[3].setHardRange(0.0, 100.0);
    props[3].setSoftRange(0.0, 75.0); props[3].setStep(0.1);
    props[3].setUnit(QStringLiteral("%"));
    props[3].setTooltip(QStringLiteral("Strength of the highlight recovery."));

    props[4].setName("Highlight Tonal Width");props[4].setType(PropertyType::Float);
    props[4].setDisplayLabel(QStringLiteral("Highlight Tonal Width"));
    props[4].setValue(QVariant(static_cast<double>(highlightTonalWidth_)));
    props[4].setDefaultValue(50.0);  props[4].setHardRange(1.0, 100.0);
    props[4].setSoftRange(5.0, 80.0); props[4].setStep(0.1);
    props[4].setUnit(QStringLiteral("%"));
    props[4].setTooltip(QStringLiteral("Luminance range affected by highlight recovery."));

    props[5].setName("Highlight Radius");     props[5].setType(PropertyType::Float);
    props[5].setDisplayLabel(QStringLiteral("Highlight Radius"));
    props[5].setValue(QVariant(static_cast<double>(highlightRadius_.value)));
    props[5].setDefaultValue(30.0);  props[5].setHardRange(0.0, 200.0);
    props[5].setSoftRange(0.0, 60.0); props[5].setStep(0.1);
    props[5].setUnit(QStringLiteral("px"));
    props[5].setTooltip(QStringLiteral("Radius used to separate highlight detail from surrounding tones."));

    props[6].setName("Color Correction");     props[6].setType(PropertyType::Float);
    props[6].setDisplayLabel(QStringLiteral("Color Correction"));
    props[6].setValue(QVariant(static_cast<double>(colorCorrection_)));
    props[6].setDefaultValue(20.0);  props[6].setHardRange(-100.0, 100.0);
    props[6].setSoftRange(-50.0, 50.0); props[6].setStep(0.1);
    props[6].setUnit(QStringLiteral("%"));
    props[6].setTooltip(QStringLiteral("Color correction applied to adjusted shadow and highlight regions."));

    props[7].setName("Midtone Contrast");     props[7].setType(PropertyType::Float);
    props[7].setDisplayLabel(QStringLiteral("Midtone Contrast"));
    props[7].setValue(QVariant(static_cast<double>(midtoneContrast_)));
    props[7].setDefaultValue(0.0);   props[7].setHardRange(-100.0, 100.0);
    props[7].setSoftRange(-50.0, 50.0); props[7].setStep(0.1);
    props[7].setUnit(QStringLiteral("%"));
    props[7].setTooltip(QStringLiteral("Contrast adjustment around the image midtones."));

    props[8].setName("Black Clip");           props[8].setType(PropertyType::Float);
    props[8].setDisplayLabel(QStringLiteral("Black Clip"));
    props[8].setValue(QVariant(static_cast<double>(blackClip_)));
    props[8].setDefaultValue(0.01);  props[8].setHardRange(0.0, 0.5);
    props[8].setSoftRange(0.0, 0.1); props[8].setStep(0.001);
    props[8].setUnit(QStringLiteral("fraction"));
    props[8].setTooltip(QStringLiteral("Fraction of the darkest values clipped to black."));

    props[9].setName("White Clip");           props[9].setType(PropertyType::Float);
    props[9].setDisplayLabel(QStringLiteral("White Clip"));
    props[9].setValue(QVariant(static_cast<double>(whiteClip_)));
    props[9].setDefaultValue(0.01);  props[9].setHardRange(0.0, 0.5);
    props[9].setSoftRange(0.0, 0.1); props[9].setStep(0.001);
    props[9].setUnit(QStringLiteral("fraction"));
    props[9].setTooltip(QStringLiteral("Fraction of the brightest values clipped to white."));

    return props;
}

void ShadowHighlightEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Shadow Amount"))         setShadowAmount(value.toFloat());
    else if (key == QStringLiteral("Shadow Tonal Width"))   setShadowTonalWidth(value.toFloat());
    else if (key == QStringLiteral("Shadow Radius"))        setShadowRadius(ArtifactCore::Units::Pixels{value.toFloat()});
    else if (key == QStringLiteral("Highlight Amount"))     setHighlightAmount(value.toFloat());
    else if (key == QStringLiteral("Highlight Tonal Width"))setHighlightTonalWidth(value.toFloat());
    else if (key == QStringLiteral("Highlight Radius"))     setHighlightRadius(ArtifactCore::Units::Pixels{value.toFloat()});
    else if (key == QStringLiteral("Color Correction"))     setColorCorrection(value.toFloat());
    else if (key == QStringLiteral("Midtone Contrast"))     setMidtoneContrast(value.toFloat());
    else if (key == QStringLiteral("Black Clip"))           setBlackClip(value.toFloat());
    else if (key == QStringLiteral("White Clip"))           setWhiteClip(value.toFloat());
    else setCommonPropertyValue(key, value);
}

} // namespace Artifact
