module;
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

#include <QString>
#include <QVariant>

module Artifact.Effect.Rasterizer.Echo;

import Artifact.Effect.Abstract;
import Artifact.Effect.Context;
import Artifact.Effect.ImplBase;
import Image.ImageF32x4RGBAWithCache;
import Image.ImageF32x4_RGBA;
import Property.Abstract;
import Utils.String.UniString;
import Core.Parallel;

namespace Artifact
{
using namespace ArtifactCore;

// ---------------------------------------------------------------------------
// CPU implementation
// ---------------------------------------------------------------------------

class EchoCPUImpl : public ArtifactEffectImplBase
{
public:
    int   echoCount_     = 4;
    float decay_         = 0.5f;
    float blendOperator_ = 0.0f;

    void applyCPU(const ImageF32x4RGBAWithCache& src,
                  ImageF32x4RGBAWithCache&        dst) override
    {
        const ImageF32x4_RGBA& srcImage = src.image();
        const float*           srcData  = srcImage.rgba32fData();

        if (!srcData || srcImage.width() <= 0 || srcImage.height() <= 0)
        {
            dst = src;
            return;
        }

        if (!context_.sampler)
        {
            dst = src.DeepCopy();
            return;
        }

        const int   width   = srcImage.width();
        const int   height  = srcImage.height();
        const int   count   = std::clamp(echoCount_, 1, 16);
        const float decay   = std::clamp(decay_, 0.0f, 1.0f);
        const float blendOp = std::clamp(blendOperator_, 0.0f, 1.0f);

        dst = src.DeepCopy();
        float* dstData = dst.image().rgba32fData();

        struct EchoLayer { ImageF32x4_RGBA image; float weight; };
        std::vector<EchoLayer> echoes;

        float totalWeight = 1.0f;
        float weight      = 1.0f;

        for (int i = 1; i <= count; ++i)
        {
            weight *= decay;
            if (weight < 0.001f) break;

            ImageF32x4RGBAWithCache sampled;
            const bool ok = context_.sampler->sampleCurrentLayerFrameRelative(
                static_cast<std::int64_t>(-i), sampled);
            if (!ok || sampled.width() <= 0 || sampled.height() <= 0
                || !sampled.image().rgba32fData())
                continue;

            echoes.push_back({ sampled.image(), weight });
            totalWeight += weight;
        }

        if (echoes.empty()) return;

        const float invTotal = 1.0f / std::max(totalWeight, 0.0001f);

        ArtifactCore::Parallel::For(0, height, [&](int y)
        {
            float* outRow = dstData + static_cast<size_t>(y) * width * 4;
            for (int x = 0; x < width; ++x)
            {
                float* pixel = outRow + static_cast<size_t>(x) * 4;
                float r = pixel[0], g = pixel[1], b = pixel[2], a = pixel[3];

                for (const auto& echo : echoes)
                {
                    const float* echoRow = echo.image.rgba32fData()
                        + static_cast<size_t>(y) * echo.image.width() * 4;
                    if (x >= echo.image.width()) continue;
                    const float* ep = echoRow + static_cast<size_t>(x) * 4;

                    if (blendOp <= 0.0001f)
                    {
                        r += ep[0] * echo.weight;
                        g += ep[1] * echo.weight;
                        b += ep[2] * echo.weight;
                        a += ep[3] * echo.weight;
                    }
                    else
                    {
                        const float t = echo.weight * blendOp;
                        r = r * (1.0f - t) + ep[0] * t;
                        g = g * (1.0f - t) + ep[1] * t;
                        b = b * (1.0f - t) + ep[2] * t;
                        a = a * (1.0f - t) + ep[3] * t;
                    }
                }

                if (blendOp <= 0.0001f)
                {
                    pixel[0] = std::clamp(r * invTotal, 0.0f, 1.0f);
                    pixel[1] = std::clamp(g * invTotal, 0.0f, 1.0f);
                    pixel[2] = std::clamp(b * invTotal, 0.0f, 1.0f);
                    pixel[3] = std::clamp(a * invTotal, 0.0f, 1.0f);
                }
                else
                {
                    pixel[0] = r; pixel[1] = g; pixel[2] = b; pixel[3] = a;
                }
            }
        });
    }
};

// ---------------------------------------------------------------------------
// EchoEffect public API
// ---------------------------------------------------------------------------

EchoEffect::EchoEffect()
    : ArtifactAbstractEffect()
{
    setPipelineStage(EffectPipelineStage::Rasterizer);
    syncImpls();
}

EchoEffect::~EchoEffect() = default;

int EchoEffect::echoCount() const { return echoCount_; }
void EchoEffect::setEchoCount(int value)
{
    echoCount_ = std::clamp(value, 1, 16);
    syncImpls();
}

float EchoEffect::decay() const { return decay_; }
void EchoEffect::setDecay(float value)
{
    decay_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

float EchoEffect::blendOperator() const { return blendOperator_; }
void EchoEffect::setBlendOperator(float value)
{
    blendOperator_ = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

std::vector<AbstractProperty> EchoEffect::getProperties() const
{
    std::vector<AbstractProperty> props;
    props.reserve(3);

    auto addInt = [&props](const char* name, int value, int minValue, int maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Integer);
        const QVariant variantValue(value);
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(minValue));
        prop.setMaxValue(QVariant(maxValue));
        props.push_back(std::move(prop));
    };

    auto addFloat = [&props](const char* name, float value, float minValue, float maxValue) {
        AbstractProperty prop;
        prop.setName(QString::fromLatin1(name));
        prop.setType(PropertyType::Float);
        const QVariant variantValue(static_cast<double>(value));
        prop.setValue(variantValue);
        prop.setDefaultValue(variantValue);
        prop.setMinValue(QVariant(static_cast<double>(minValue)));
        prop.setMaxValue(QVariant(static_cast<double>(maxValue)));
        props.push_back(std::move(prop));
    };

    addInt("echoCount", echoCount_, 1, 16);
    addFloat("decay", decay_, 0.0f, 1.0f);
    addFloat("blendOperator", blendOperator_, 0.0f, 1.0f);
    return props;
}

void EchoEffect::setPropertyValue(const UniString& name, const QVariant& value)
{
    const QString key = name.toQString();
    if (key == QStringLiteral("echoCount"))
        setEchoCount(value.toInt());
    else if (key == QStringLiteral("decay"))
        setDecay(value.toFloat());
    else if (key == QStringLiteral("blendOperator"))
        setBlendOperator(value.toFloat());
}

void EchoEffect::syncImpls()
{
    auto cpu = std::make_shared<EchoCPUImpl>();
    cpu->echoCount_     = echoCount_;
    cpu->decay_         = decay_;
    cpu->blendOperator_ = blendOperator_;
    setCPUImpl(cpu);
}

} // namespace Artifact
