module;
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QHashFunctions>
#include <QJsonObject>
#include <QMatrix4x4>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QTransform>
#include <QVariant>
#include <QVector3D>
#include <wobjectimpl.h>

module Artifact.Layer.FormParticle;

import std;
import Artifact.Composition.Abstract;
import Artifact.Render.IRenderer;

import Graphics.ParticleData;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Property.Abstract;
import Property.Group;

namespace Artifact {

namespace {

template <typename T>
T clampValue(T value, T minValue, T maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

static float hashToUnit(std::uint32_t value)
{
    value ^= value >> 17;
    value *= 0xed5ad4bbU;
    value ^= value >> 11;
    value *= 0xac4c1b51U;
    value ^= value >> 15;
    value *= 0x31848babU;
    value ^= value >> 14;
    return static_cast<float>(value & 0x00ffffffU) / static_cast<float>(0x01000000U - 1U);
}

static QColor lerpColor(const QColor& a, const QColor& b, float t)
{
    const float clamped = clampValue(t, 0.0f, 1.0f);
    QColor result;
    result.setRedF(a.redF() + (b.redF() - a.redF()) * clamped);
    result.setGreenF(a.greenF() + (b.greenF() - a.greenF()) * clamped);
    result.setBlueF(a.blueF() + (b.blueF() - a.blueF()) * clamped);
    result.setAlphaF(a.alphaF() + (b.alphaF() - a.alphaF()) * clamped);
    return result;
}

static quint64 mixSignature(quint64 seed, quint64 value)
{
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
}

static float normalizedIndex(int index, int count)
{
    if (count <= 1) {
        return 0.0f;
    }
    return clampValue(static_cast<float>(index) / static_cast<float>(count - 1), 0.0f, 1.0f);
}

static QRectF computeBounds(const FormParticleSettings& settings)
{
    const int columns = std::max(1, settings.columns);
    const int rows = std::max(1, settings.rows);
    const int depth = std::max(1, settings.depth);
    const float width = std::max(1.0f, static_cast<float>(columns - 1) * std::max(1.0f, settings.spacingX) + settings.particleSize * 2.0f);
    const float height = std::max(1.0f, static_cast<float>(rows - 1) * std::max(1.0f, settings.spacingY) + settings.particleSize * 2.0f);
    const float maxDepth = std::max(1.0f, static_cast<float>(depth - 1) * std::max(1.0f, settings.spacingZ) + settings.particleSize * 2.0f);
    return QRectF(-width * 0.5, -height * 0.5, width, std::max(height, maxDepth));
}

static QString presetNameNormalized(const QString& preset)
{
    return preset.trimmed().toLower();
}

} // namespace

FormParticleSettings::FormParticleSettings()
{
    renderSettings.blendMode = FormParticleBlendMode::Alpha;
    renderSettings.billboardMode = FormParticleBillboardMode::ScreenAligned;
    renderSettings.sortMode = FormParticleSortMode::None;
    renderSettings.depthTest = false;
    renderSettings.depthWrite = false;
}

QJsonObject FormParticleSettings::toJson() const
{
    QJsonObject obj;
    obj["version"] = 1;
    obj["generatorMode"] = static_cast<int>(generatorMode);
    obj["colorMode"] = static_cast<int>(colorMode);
    obj["originMode"] = static_cast<int>(originMode);
    obj["columns"] = columns;
    obj["rows"] = rows;
    obj["depth"] = depth;
    obj["maxParticles"] = maxParticles;
    obj["spacingX"] = spacingX;
    obj["spacingY"] = spacingY;
    obj["spacingZ"] = spacingZ;
    obj["particleSize"] = particleSize;
    obj["particleOpacity"] = particleOpacity;
    obj["noiseAmount"] = noiseAmount;
    obj["noiseScale"] = noiseScale;
    obj["noiseSpeed"] = noiseSpeed;
    obj["noisePhase"] = noisePhase;
    obj["twistAmount"] = twistAmount;
    obj["falloff"] = falloff;
    obj["seed"] = static_cast<double>(seed);
    obj["sourcePath"] = sourcePath;
    obj["sourceAlphaThreshold"] = sourceAlphaThreshold;
    obj["sourceLumaThreshold"] = sourceLumaThreshold;
    obj["solidColor"] = solidColor.name(QColor::HexArgb);
    obj["gradientStartColor"] = gradientStartColor.name(QColor::HexArgb);
    obj["gradientEndColor"] = gradientEndColor.name(QColor::HexArgb);

    QJsonObject render;
    render["blendMode"] = static_cast<int>(renderSettings.blendMode);
    render["billboardMode"] = static_cast<int>(renderSettings.billboardMode);
    render["sortMode"] = static_cast<int>(renderSettings.sortMode);
    render["depthTest"] = renderSettings.depthTest;
    render["depthWrite"] = renderSettings.depthWrite;
    render["softParticles"] = renderSettings.softParticles;
    obj["renderSettings"] = render;
    return obj;
}

void FormParticleSettings::fromJson(const QJsonObject& obj)
{
    if (obj.contains("generatorMode")) {
        generatorMode = static_cast<GeneratorMode>(
            clampValue(obj.value("generatorMode").toInt(static_cast<int>(generatorMode)), 0, 2));
    }
    if (obj.contains("colorMode")) {
        colorMode = static_cast<ColorMode>(
            clampValue(obj.value("colorMode").toInt(static_cast<int>(colorMode)), 0, 2));
    }
    if (obj.contains("originMode")) {
        originMode = static_cast<OriginMode>(
            clampValue(obj.value("originMode").toInt(static_cast<int>(originMode)), 0, 2));
    }
    if (obj.contains("columns")) {
        columns = clampValue(obj.value("columns").toInt(columns), 1, 512);
    }
    if (obj.contains("rows")) {
        rows = clampValue(obj.value("rows").toInt(rows), 1, 512);
    }
    if (obj.contains("depth")) {
        depth = clampValue(obj.value("depth").toInt(depth), 1, 128);
    }
    if (obj.contains("maxParticles")) {
        maxParticles = clampValue(obj.value("maxParticles").toInt(maxParticles), 1, 100000);
    }
    if (obj.contains("spacingX")) {
        spacingX = std::max(0.01f, static_cast<float>(obj.value("spacingX").toDouble(spacingX)));
    }
    if (obj.contains("spacingY")) {
        spacingY = std::max(0.01f, static_cast<float>(obj.value("spacingY").toDouble(spacingY)));
    }
    if (obj.contains("spacingZ")) {
        spacingZ = std::max(0.01f, static_cast<float>(obj.value("spacingZ").toDouble(spacingZ)));
    }
    if (obj.contains("particleSize")) {
        particleSize = std::max(0.1f, static_cast<float>(obj.value("particleSize").toDouble(particleSize)));
    }
    if (obj.contains("particleOpacity")) {
        particleOpacity = clampValue(static_cast<float>(obj.value("particleOpacity").toDouble(particleOpacity)), 0.0f, 1.0f);
    }
    if (obj.contains("noiseAmount")) {
        noiseAmount = std::max(0.0f, static_cast<float>(obj.value("noiseAmount").toDouble(noiseAmount)));
    }
    if (obj.contains("noiseScale")) {
        noiseScale = std::max(0.0f, static_cast<float>(obj.value("noiseScale").toDouble(noiseScale)));
    }
    if (obj.contains("noiseSpeed")) {
        noiseSpeed = static_cast<float>(obj.value("noiseSpeed").toDouble(noiseSpeed));
    }
    if (obj.contains("noisePhase")) {
        noisePhase = static_cast<float>(obj.value("noisePhase").toDouble(noisePhase));
    }
    if (obj.contains("sourcePath")) {
        sourcePath = obj.value("sourcePath").toString();
    }
    if (obj.contains("sourceAlphaThreshold")) {
        sourceAlphaThreshold = clampValue(
            static_cast<float>(obj.value("sourceAlphaThreshold").toDouble(sourceAlphaThreshold)),
            0.0f,
            1.0f);
    }
    if (obj.contains("sourceLumaThreshold")) {
        sourceLumaThreshold = clampValue(
            static_cast<float>(obj.value("sourceLumaThreshold").toDouble(sourceLumaThreshold)),
            0.0f,
            1.0f);
    }
    if (obj.contains("twistAmount")) {
        twistAmount = static_cast<float>(obj.value("twistAmount").toDouble(twistAmount));
    }
    if (obj.contains("falloff")) {
        falloff = std::max(0.0f, static_cast<float>(obj.value("falloff").toDouble(falloff)));
    }
    if (obj.contains("seed")) {
        seed = static_cast<std::uint32_t>(std::max<qint64>(0, obj.value("seed").toVariant().toLongLong()));
    }
    if (obj.contains("solidColor")) {
        solidColor = QColor(obj.value("solidColor").toString());
    }
    if (obj.contains("gradientStartColor")) {
        gradientStartColor = QColor(obj.value("gradientStartColor").toString());
    }
    if (obj.contains("gradientEndColor")) {
        gradientEndColor = QColor(obj.value("gradientEndColor").toString());
    }
    if (obj.contains("renderSettings") && obj.value("renderSettings").isObject()) {
        const QJsonObject render = obj.value("renderSettings").toObject();
        if (render.contains("blendMode")) {
            renderSettings.blendMode = static_cast<FormParticleBlendMode>(
                clampValue(render.value("blendMode").toInt(static_cast<int>(renderSettings.blendMode)), 0, 4));
        }
        if (render.contains("billboardMode")) {
            renderSettings.billboardMode = static_cast<FormParticleBillboardMode>(
                clampValue(render.value("billboardMode").toInt(static_cast<int>(renderSettings.billboardMode)), 0, 3));
        }
        if (render.contains("sortMode")) {
            renderSettings.sortMode = static_cast<FormParticleSortMode>(
                clampValue(render.value("sortMode").toInt(static_cast<int>(renderSettings.sortMode)), 0, 3));
        }
        if (render.contains("depthTest")) {
            renderSettings.depthTest = render.value("depthTest").toBool(renderSettings.depthTest);
        }
        if (render.contains("depthWrite")) {
            renderSettings.depthWrite = render.value("depthWrite").toBool(renderSettings.depthWrite);
        }
        if (render.contains("softParticles")) {
            renderSettings.softParticles = render.value("softParticles").toBool(renderSettings.softParticles);
        }
    }
}

class ArtifactFormParticleLayer::Impl {
public:
    FormParticleSettings settings;
    mutable bool cacheDirty = true;
    mutable quint64 cachedSignature = 0;
    mutable qint64 cachedFrame = std::numeric_limits<qint64>::min();
    mutable ArtifactCore::ParticleRenderData cachedRenderData;
    mutable ArtifactCore::ImageF32x4_RGBA sourceImage;
    mutable QString loadedSourcePath;
    mutable bool sourceLoadAttempted = false;
    mutable bool sourceLoaded = false;
    mutable qint64 loadedSourceModifiedMs = -1;
    mutable quint64 sourceRevision = 0;
    mutable quint64 cachedBaseSignature = 0;
    mutable std::vector<QVector3D> cachedBasePoints;

    void markDirty()
    {
        cacheDirty = true;
    }

    const ArtifactCore::ImageF32x4_RGBA* layerMapSource() const
    {
        if (settings.generatorMode != FormParticleSettings::GeneratorMode::LayerMap ||
            settings.sourcePath.isEmpty()) {
            return nullptr;
        }
        const QFileInfo sourceInfo(settings.sourcePath);
        const qint64 modifiedMs = sourceInfo.exists()
            ? sourceInfo.lastModified().toMSecsSinceEpoch()
            : -1;
        if (!sourceLoadAttempted ||
            loadedSourcePath != settings.sourcePath ||
            loadedSourceModifiedMs != modifiedMs) {
            sourceLoadAttempted = true;
            loadedSourcePath = settings.sourcePath;
            loadedSourceModifiedMs = modifiedMs;
            sourceLoaded = sourceImage.load(settings.sourcePath) && !sourceImage.isEmpty();
            ++sourceRevision;
            cacheDirty = true;
        }
        return sourceLoaded ? &sourceImage : nullptr;
    }

    quint64 signatureForFrame(qint64 frameNumber, const QTransform& transform) const
    {
        quint64 sig = 0xcbf29ce484222325ULL;
        auto mixInt = [&sig](quint64 value) { sig = mixSignature(sig, value); };
        mixInt(static_cast<quint64>(frameNumber));
        mixInt(static_cast<quint64>(std::lround(transform.m11() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m12() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m13() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m21() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m22() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m23() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m31() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m32() * 100000.0)));
        mixInt(static_cast<quint64>(std::lround(transform.m33() * 100000.0)));
        mixInt(static_cast<quint64>(settings.generatorMode));
        mixInt(static_cast<quint64>(settings.colorMode));
        mixInt(static_cast<quint64>(settings.originMode));
        mixInt(static_cast<quint64>(settings.columns));
        mixInt(static_cast<quint64>(settings.rows));
        mixInt(static_cast<quint64>(settings.depth));
        mixInt(static_cast<quint64>(settings.maxParticles));
        mixInt(static_cast<quint64>(std::lround(settings.spacingX * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.spacingY * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.spacingZ * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.particleSize * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.particleOpacity * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.noiseAmount * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.noiseScale * 100000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.noiseSpeed * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.noisePhase * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.twistAmount * 1000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.falloff * 1000.0f)));
        mixInt(static_cast<quint64>(settings.seed));
        mixInt(static_cast<quint64>(qHash(settings.sourcePath)));
        mixInt(static_cast<quint64>(std::lround(settings.sourceAlphaThreshold * 100000.0f)));
        mixInt(static_cast<quint64>(std::lround(settings.sourceLumaThreshold * 100000.0f)));
        mixInt(static_cast<quint64>(settings.solidColor.rgba()));
        mixInt(static_cast<quint64>(settings.gradientStartColor.rgba()));
        mixInt(static_cast<quint64>(settings.gradientEndColor.rgba()));
        mixInt(static_cast<quint64>(settings.renderSettings.blendMode));
        mixInt(static_cast<quint64>(settings.renderSettings.billboardMode));
        mixInt(static_cast<quint64>(settings.renderSettings.sortMode));
        mixInt(static_cast<quint64>(settings.renderSettings.depthTest));
        mixInt(static_cast<quint64>(settings.renderSettings.depthWrite));
        mixInt(static_cast<quint64>(settings.renderSettings.softParticles));
        mixInt(sourceRevision);
        return sig;
    }

    quint64 baseSignature() const
    {
        quint64 sig = 0xcbf29ce484222325ULL;
        sig = mixSignature(sig, static_cast<quint64>(settings.generatorMode));
        sig = mixSignature(sig, static_cast<quint64>(settings.originMode));
        sig = mixSignature(sig, static_cast<quint64>(settings.columns));
        sig = mixSignature(sig, static_cast<quint64>(settings.rows));
        sig = mixSignature(sig, static_cast<quint64>(settings.depth));
        sig = mixSignature(sig, static_cast<quint64>(settings.maxParticles));
        sig = mixSignature(sig, static_cast<quint64>(std::lround(settings.spacingX * 1000.0f)));
        sig = mixSignature(sig, static_cast<quint64>(std::lround(settings.spacingY * 1000.0f)));
        sig = mixSignature(sig, static_cast<quint64>(std::lround(settings.spacingZ * 1000.0f)));
        return sig;
    }

    const std::vector<QVector3D>& basePoints() const
    {
        const quint64 signature = baseSignature();
        if (signature == cachedBaseSignature && !cachedBasePoints.empty()) {
            return cachedBasePoints;
        }

        const int columns = clampValue(settings.columns, 1, 512);
        const int rows = clampValue(settings.rows, 1, 512);
        const int depth = settings.generatorMode == FormParticleSettings::GeneratorMode::Grid3D
            ? clampValue(settings.depth, 1, 128)
            : 1;
        const std::size_t latticeCount =
            static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) *
            static_cast<std::size_t>(depth);
        const std::size_t cacheLimit =
            settings.generatorMode == FormParticleSettings::GeneratorMode::LayerMap
            ? latticeCount
            : std::min<std::size_t>(
                  latticeCount,
                  static_cast<std::size_t>(clampValue(settings.maxParticles, 1, 100000)));

        cachedBasePoints.clear();
        cachedBasePoints.reserve(cacheLimit);
        const float offsetX = settings.originMode == FormParticleSettings::OriginMode::Center
            ? static_cast<float>(columns - 1) * settings.spacingX * 0.5f
            : 0.0f;
        const float offsetY = settings.originMode == FormParticleSettings::OriginMode::Center
            ? static_cast<float>(rows - 1) * settings.spacingY * 0.5f
            : 0.0f;
        const float offsetZ = settings.originMode == FormParticleSettings::OriginMode::Center
            ? static_cast<float>(depth - 1) * settings.spacingZ * 0.5f
            : 0.0f;

        for (int z = 0; z < depth && cachedBasePoints.size() < cacheLimit; ++z) {
            for (int y = 0; y < rows && cachedBasePoints.size() < cacheLimit; ++y) {
                for (int x = 0; x < columns && cachedBasePoints.size() < cacheLimit; ++x) {
                    cachedBasePoints.emplace_back(
                        static_cast<float>(x) * settings.spacingX - offsetX,
                        static_cast<float>(y) * settings.spacingY - offsetY,
                        static_cast<float>(z) * settings.spacingZ - offsetZ);
                }
            }
        }
        cachedBaseSignature = signature;
        return cachedBasePoints;
    }

    void syncSourceSize(ArtifactFormParticleLayer* layer) const
    {
        if (!layer) {
            return;
        }
        const QRectF bounds = computeBounds(settings);
        layer->setSourceSize(Size_2D(std::max(1, static_cast<int>(std::ceil(bounds.width()))),
                                     std::max(1, static_cast<int>(std::ceil(bounds.height())))));
        layer->setIs3D(settings.generatorMode == FormParticleSettings::GeneratorMode::Grid3D);
    }
};

ArtifactFormParticleLayer::ArtifactFormParticleLayer()
    : impl_(new Impl())
{
    impl_->settings = FormParticleSettings{};
    impl_->syncSourceSize(this);
}

ArtifactFormParticleLayer::~ArtifactFormParticleLayer()
{
    delete impl_;
}

static QVector3D applyField(const FormParticleSettings& settings,
                            const QVector3D& base,
                            std::uint32_t index,
                            float timeSeconds)
{
    QVector3D result = base;
    const float seedPhase = static_cast<float>(settings.seed % 104729U) * 0.0137f;
    const float phase = settings.noisePhase + timeSeconds * settings.noiseSpeed + seedPhase;
    const float falloffRadius = settings.falloff > 0.0f ? settings.falloff : 1.0f;
    const float distance = std::sqrt(base.x() * base.x() + base.y() * base.y() + base.z() * base.z());
    const float falloff = settings.falloff > 0.0f ? clampValue(1.0f - (distance / falloffRadius), 0.0f, 1.0f) : 1.0f;
    const float noiseAmp = settings.noiseAmount * falloff;
    const float hash = hashToUnit(static_cast<std::uint32_t>(settings.seed + index * 2654435761U));
    const float wobble = std::sin(base.x() * settings.noiseScale + phase + hash * 6.2831853f);
    const float wobble2 = std::cos(base.y() * settings.noiseScale * 1.17f + phase * 1.13f + hash * 3.1415926f);
    const float wobble3 = std::sin((base.x() + base.y() + base.z()) * settings.noiseScale * 0.71f + phase * 0.83f);

    result.setX(result.x() + wobble * noiseAmp);
    result.setY(result.y() + wobble2 * noiseAmp);
    result.setZ(result.z() + wobble3 * noiseAmp * 0.5f);

    if (settings.twistAmount != 0.0f) {
        const float angle = settings.twistAmount * falloff * 0.0008f * (distance + timeSeconds * 120.0f);
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        const float x = result.x();
        const float y = result.y();
        result.setX(x * c - y * s);
        result.setY(x * s + y * c);
    }

    return result;
}

static QColor colorForPoint(const FormParticleSettings& settings,
                            int column,
                            int row,
                            int depth,
                            int columns,
                            int rows,
                            int depthCount)
{
    switch (settings.colorMode) {
    case FormParticleSettings::ColorMode::AxisGradient: {
        const float t = clampValue((normalizedIndex(column, columns) + normalizedIndex(row, rows) + normalizedIndex(depth, depthCount)) / 3.0f, 0.0f, 1.0f);
        return lerpColor(settings.gradientStartColor, settings.gradientEndColor, t);
    }
    case FormParticleSettings::ColorMode::SourceColor:
        return settings.solidColor;
    case FormParticleSettings::ColorMode::Solid:
    default:
        return settings.solidColor;
    }
}

static ArtifactCore::ParticleRenderData buildRenderData(const FormParticleSettings& settings,
                                                        qint64 frameNumber,
                                                        float timeSeconds,
                                                        const QTransform& transform,
                                                        const ArtifactCore::ImageF32x4_RGBA* layerMapSource,
                                                        const std::vector<QVector3D>& basePoints)
{
    ArtifactCore::ParticleRenderData data;
    data.frameNumber = frameNumber;
    data.options.blend = static_cast<ArtifactCore::ParticleBlendPolicy>(
        clampValue(static_cast<int>(settings.renderSettings.blendMode), 0, 4));
    data.options.billboard = static_cast<ArtifactCore::ParticleBillboardPolicy>(
        clampValue(static_cast<int>(settings.renderSettings.billboardMode), 0, 3));
    data.options.depthTest = settings.renderSettings.depthTest;
    data.options.depthWrite =
        settings.renderSettings.depthTest && settings.renderSettings.depthWrite;
    if (settings.generatorMode == FormParticleSettings::GeneratorMode::LayerMap &&
        (!layerMapSource || layerMapSource->isEmpty())) {
        return data;
    }
    const int columns = clampValue(settings.columns, 1, 512);
    const int rows = clampValue(settings.rows, 1, 512);
    const int depth = settings.generatorMode == FormParticleSettings::GeneratorMode::Grid3D
        ? clampValue(settings.depth, 1, 128)
        : 1;
    const int limit = clampValue(settings.maxParticles, 1, 100000);
    const std::size_t latticeCount =
        static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows) *
        static_cast<std::size_t>(depth);
    data.particles.reserve(std::min<std::size_t>(
        static_cast<std::size_t>(limit),
        latticeCount));

    for (std::size_t point = 0;
         point < basePoints.size() && static_cast<int>(data.particles.size()) < limit;
         ++point) {
                const std::uint32_t pointIndex = static_cast<std::uint32_t>(point);
                const int x = static_cast<int>(point % static_cast<std::size_t>(columns));
                const int y = static_cast<int>(
                    (point / static_cast<std::size_t>(columns)) %
                    static_cast<std::size_t>(rows));
                const int z = static_cast<int>(
                    point /
                    (static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows)));
                const QVector3D& base = basePoints[point];

                QColor sourceColor;
                float sourceOpacity = 1.0f;
                if (settings.generatorMode == FormParticleSettings::GeneratorMode::LayerMap) {
                    const int sourceX = columns <= 1
                        ? 0
                        : static_cast<int>(std::lround(
                              normalizedIndex(x, columns) * static_cast<float>(layerMapSource->width() - 1)));
                    const int sourceY = rows <= 1
                        ? 0
                        : static_cast<int>(std::lround(
                              normalizedIndex(y, rows) * static_cast<float>(layerMapSource->height() - 1)));
                    const auto sample = layerMapSource->getPixel(sourceX, sourceY);
                    sourceOpacity = clampValue(sample.a(), 0.0f, 1.0f);
                    const float luma = clampValue(
                        sample.r() * 0.2126f + sample.g() * 0.7152f + sample.b() * 0.0722f,
                        0.0f,
                        1.0f);
                    if (sourceOpacity < settings.sourceAlphaThreshold ||
                        luma < settings.sourceLumaThreshold) {
                        continue;
                    }
                    sourceColor = QColor::fromRgbF(
                        clampValue(sample.r(), 0.0f, 1.0f),
                        clampValue(sample.g(), 0.0f, 1.0f),
                        clampValue(sample.b(), 0.0f, 1.0f),
                        1.0f);
                }

                QVector3D displaced = applyField(settings, base, pointIndex, timeSeconds);
                const QPointF mapped = transform.map(QPointF(displaced.x(), displaced.y()));

                ArtifactCore::ParticleVertex vertex;
                vertex.px = static_cast<float>(mapped.x());
                vertex.py = static_cast<float>(mapped.y());
                vertex.pz = displaced.z();
                vertex.vx = displaced.x() - base.x();
                vertex.vy = displaced.y() - base.y();
                vertex.vz = displaced.z() - base.z();
                const QColor color =
                    settings.generatorMode == FormParticleSettings::GeneratorMode::LayerMap &&
                            settings.colorMode == FormParticleSettings::ColorMode::SourceColor
                        ? sourceColor
                        : colorForPoint(settings, x, y, z, columns, rows, depth);
                vertex.r = color.redF();
                vertex.g = color.greenF();
                vertex.b = color.blueF();
                vertex.a = clampValue(
                    color.alphaF() * settings.particleOpacity * sourceOpacity,
                    0.0f,
                    1.0f);
                vertex.size = std::max(0.1f, settings.particleSize);
                vertex.stretch = 1.0f;
                vertex.rotation = 0.0f;
                vertex.age = static_cast<float>(pointIndex);
                vertex.lifetime = static_cast<float>(std::max<std::size_t>(1u, latticeCount));
                data.particles.push_back(vertex);
    }

    if (settings.renderSettings.sortMode == FormParticleSortMode::Distance) {
        std::stable_sort(
            data.particles.begin(),
            data.particles.end(),
            [](const ArtifactCore::ParticleVertex& a,
               const ArtifactCore::ParticleVertex& b) {
                return a.pz > b.pz;
            });
    } else if (settings.renderSettings.sortMode == FormParticleSortMode::OldestFirst) {
        std::stable_sort(
            data.particles.begin(),
            data.particles.end(),
            [](const ArtifactCore::ParticleVertex& a,
               const ArtifactCore::ParticleVertex& b) {
                return a.age < b.age;
            });
    } else if (settings.renderSettings.sortMode == FormParticleSortMode::YoungestFirst) {
        std::stable_sort(
            data.particles.begin(),
            data.particles.end(),
            [](const ArtifactCore::ParticleVertex& a,
               const ArtifactCore::ParticleVertex& b) {
                return a.age > b.age;
            });
    }

    return data;
}

void ArtifactFormParticleLayer::draw(ArtifactIRenderer* renderer)
{
    if (!renderer) {
        return;
    }

    const qint64 frameNumber = currentFrame();
    const auto comp = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    const float fps = comp
                          ? std::max(1.0f, static_cast<float>(comp->frameRate().framerate()))
                          : 30.0f;
    const float timeSeconds = static_cast<float>(frameNumber) / fps;
    const QTransform transform = getGlobalTransform();
    const auto* layerMapSource = impl_->layerMapSource();
    const quint64 signature = impl_->signatureForFrame(frameNumber, transform);
    if (impl_->cacheDirty || impl_->cachedFrame != frameNumber || impl_->cachedSignature != signature) {
        impl_->cachedRenderData = buildRenderData(
            impl_->settings,
            frameNumber,
            timeSeconds,
            transform,
            layerMapSource,
            impl_->basePoints());
        impl_->cachedSignature = signature;
        impl_->cachedFrame = frameNumber;
        impl_->cacheDirty = false;
    }

    if (impl_->cachedRenderData.particles.empty()) {
        return;
    }

    renderer->drawParticles(impl_->cachedRenderData);
}

QRectF ArtifactFormParticleLayer::localBounds() const
{
    return computeBounds(impl_->settings);
}

QString ArtifactFormParticleLayer::debugState() const
{
    const QString sourceState =
        impl_->settings.generatorMode == FormParticleSettings::GeneratorMode::LayerMap
        ? (impl_->settings.sourcePath.isEmpty()
               ? QStringLiteral("missing")
               : (impl_->sourceLoaded
                      ? QStringLiteral("loaded")
                      : (impl_->sourceLoadAttempted
                             ? QStringLiteral("failed")
                             : QStringLiteral("pending"))))
        : QStringLiteral("n/a");
    return QStringLiteral("FormParticleLayer{mode=%1 grid=%2x%3x%4 particles=%5 baseCache=%6 noise=%7 twist=%8 source=%9}")
        .arg(static_cast<int>(impl_->settings.generatorMode))
        .arg(impl_->settings.columns)
        .arg(impl_->settings.rows)
        .arg(impl_->settings.depth)
        .arg(impl_->settings.maxParticles)
        .arg(static_cast<qulonglong>(impl_->cachedBasePoints.size()))
        .arg(impl_->settings.noiseAmount, 0, 'f', 2)
        .arg(impl_->settings.twistAmount, 0, 'f', 2)
        .arg(sourceState);
}

QJsonObject ArtifactFormParticleLayer::toJson() const
{
    QJsonObject json = ArtifactAbstractLayer::toJson();
    json["type"] = static_cast<int>(LayerType::FormParticle);
    json["layerType"] = QStringLiteral("FormParticleLayer");
    json["isFormParticleLayer"] = true;
    json["formParticle"] = impl_->settings.toJson();
    return json;
}

ArtifactAbstractLayerPtr ArtifactFormParticleLayer::fromJson(const QJsonObject& obj)
{
    auto layer = std::make_shared<ArtifactFormParticleLayer>();
    layer->applyPropertiesFromJson(obj);
    return layer;
}

void ArtifactFormParticleLayer::loadPreset(const QString& presetName)
{
    const QString preset = presetNameNormalized(presetName);
    if (preset == QStringLiteral("starfield") || preset == QStringLiteral("starvolume")) {
        impl_->settings.generatorMode = FormParticleSettings::GeneratorMode::Grid3D;
        impl_->settings.columns = 28;
        impl_->settings.rows = 18;
        impl_->settings.depth = 12;
        impl_->settings.spacingX = 28.0f;
        impl_->settings.spacingY = 28.0f;
        impl_->settings.spacingZ = 18.0f;
        impl_->settings.particleSize = 3.0f;
        impl_->settings.colorMode = FormParticleSettings::ColorMode::AxisGradient;
        impl_->settings.solidColor = QColor(255, 255, 255);
        impl_->settings.gradientStartColor = QColor(255, 224, 120);
        impl_->settings.gradientEndColor = QColor(101, 220, 255);
        impl_->settings.noiseAmount = 10.0f;
        impl_->settings.noiseScale = 0.012f;
        impl_->settings.noiseSpeed = 0.8f;
        impl_->settings.twistAmount = 2.2f;
        impl_->settings.renderSettings.blendMode = FormParticleBlendMode::Additive;
        impl_->settings.renderSettings.depthTest = true;
    } else if (preset == QStringLiteral("digitalsand") || preset == QStringLiteral("sand")) {
        impl_->settings.generatorMode = FormParticleSettings::GeneratorMode::Grid2D;
        impl_->settings.columns = 96;
        impl_->settings.rows = 54;
        impl_->settings.depth = 1;
        impl_->settings.spacingX = 14.0f;
        impl_->settings.spacingY = 14.0f;
        impl_->settings.particleSize = 2.0f;
        impl_->settings.colorMode = FormParticleSettings::ColorMode::Solid;
        impl_->settings.solidColor = QColor(255, 220, 120);
        impl_->settings.noiseAmount = 22.0f;
        impl_->settings.noiseScale = 0.03f;
        impl_->settings.noiseSpeed = 1.2f;
        impl_->settings.twistAmount = 0.75f;
        impl_->settings.renderSettings.blendMode = FormParticleBlendMode::Alpha;
    } else if (preset == QStringLiteral("wavematrix") || preset == QStringLiteral("wave")) {
        impl_->settings.generatorMode = FormParticleSettings::GeneratorMode::Grid2D;
        impl_->settings.columns = 72;
        impl_->settings.rows = 40;
        impl_->settings.spacingX = 18.0f;
        impl_->settings.spacingY = 18.0f;
        impl_->settings.particleSize = 3.0f;
        impl_->settings.colorMode = FormParticleSettings::ColorMode::AxisGradient;
        impl_->settings.gradientStartColor = QColor(74, 196, 255);
        impl_->settings.gradientEndColor = QColor(255, 118, 202);
        impl_->settings.noiseAmount = 12.0f;
        impl_->settings.noiseScale = 0.02f;
        impl_->settings.noiseSpeed = 1.0f;
        impl_->settings.twistAmount = 1.8f;
        impl_->settings.renderSettings.blendMode = FormParticleBlendMode::Screen;
    } else if (preset == QStringLiteral("sourcepixels") || preset == QStringLiteral("source")) {
        impl_->settings = FormParticleSettings{};
        impl_->settings.generatorMode = FormParticleSettings::GeneratorMode::LayerMap;
        impl_->settings.columns = 96;
        impl_->settings.rows = 54;
        impl_->settings.spacingX = 12.0f;
        impl_->settings.spacingY = 12.0f;
        impl_->settings.particleSize = 5.0f;
        impl_->settings.colorMode = FormParticleSettings::ColorMode::SourceColor;
        impl_->settings.sourceAlphaThreshold = 0.05f;
        impl_->settings.sourceLumaThreshold = 0.05f;
        impl_->settings.renderSettings.blendMode = FormParticleBlendMode::Alpha;
    } else {
        impl_->settings = FormParticleSettings{};
    }
    impl_->syncSourceSize(this);
    setDirty();
    impl_->markDirty();
    Q_EMIT changed();
}

QStringList ArtifactFormParticleLayer::availablePresets() const
{
    return {
        QStringLiteral("dotGrid"),
        QStringLiteral("starVolume"),
        QStringLiteral("digitalSand"),
        QStringLiteral("waveMatrix"),
        QStringLiteral("sourcePixels")
    };
}

std::vector<ArtifactCore::PropertyGroup> ArtifactFormParticleLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();

    auto makeProp = [this](const QString& name, ArtifactCore::PropertyType type, const QVariant& value, int priority = 0) {
        return persistentLayerProperty(name, type, value, priority);
    };

    ArtifactCore::PropertyGroup formGroup(QStringLiteral("Form"));
    auto modeProp = makeProp(QStringLiteral("form.mode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.generatorMode), -240);
    modeProp->setDisplayLabel(QStringLiteral("Mode"));
    modeProp->setTooltip(QStringLiteral("0=Grid2D, 1=Grid3D, 2=LayerMap"));
    formGroup.addProperty(modeProp);
    formGroup.addProperty(makeProp(QStringLiteral("form.columns"), ArtifactCore::PropertyType::Integer, impl_->settings.columns, -239));
    formGroup.addProperty(makeProp(QStringLiteral("form.rows"), ArtifactCore::PropertyType::Integer, impl_->settings.rows, -238));
    formGroup.addProperty(makeProp(QStringLiteral("form.depth"), ArtifactCore::PropertyType::Integer, impl_->settings.depth, -237));
    auto spacingXProp = makeProp(QStringLiteral("form.spacingX"), ArtifactCore::PropertyType::Float, impl_->settings.spacingX, -236);
    spacingXProp->setUnit(QStringLiteral("px"));
    formGroup.addProperty(spacingXProp);
    auto spacingYProp = makeProp(QStringLiteral("form.spacingY"), ArtifactCore::PropertyType::Float, impl_->settings.spacingY, -235);
    spacingYProp->setUnit(QStringLiteral("px"));
    formGroup.addProperty(spacingYProp);
    auto spacingZProp = makeProp(QStringLiteral("form.spacingZ"), ArtifactCore::PropertyType::Float, impl_->settings.spacingZ, -234);
    spacingZProp->setUnit(QStringLiteral("px"));
    formGroup.addProperty(spacingZProp);
    auto maxParticlesProp = makeProp(QStringLiteral("form.maxParticles"), ArtifactCore::PropertyType::Integer, impl_->settings.maxParticles, -233);
    maxParticlesProp->setHardRange(1, 100000);
    maxParticlesProp->setSoftRange(1, 16384);
    formGroup.addProperty(maxParticlesProp);
    auto originProp = makeProp(QStringLiteral("form.originMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.originMode), -232);
    originProp->setTooltip(QStringLiteral("0=Center, 1=TopLeft, 2=LayerBounds"));
    formGroup.addProperty(originProp);
    groups.push_back(formGroup);

    ArtifactCore::PropertyGroup sourceGroup(QStringLiteral("Source"));
    sourceGroup.addProperty(makeProp(
        QStringLiteral("source.path"),
        ArtifactCore::PropertyType::String,
        impl_->settings.sourcePath,
        -228));
    auto alphaThresholdProp = makeProp(
        QStringLiteral("source.alphaThreshold"),
        ArtifactCore::PropertyType::Float,
        impl_->settings.sourceAlphaThreshold,
        -227);
    alphaThresholdProp->setHardRange(0.0, 1.0);
    alphaThresholdProp->setStep(0.01);
    sourceGroup.addProperty(alphaThresholdProp);
    auto lumaThresholdProp = makeProp(
        QStringLiteral("source.lumaThreshold"),
        ArtifactCore::PropertyType::Float,
        impl_->settings.sourceLumaThreshold,
        -226);
    lumaThresholdProp->setHardRange(0.0, 1.0);
    lumaThresholdProp->setStep(0.01);
    sourceGroup.addProperty(lumaThresholdProp);
    groups.push_back(sourceGroup);

    ArtifactCore::PropertyGroup particleGroup(QStringLiteral("Particle"));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.size"), ArtifactCore::PropertyType::Float, impl_->settings.particleSize, -220));
    auto opacityProp = makeProp(QStringLiteral("particle.opacity"), ArtifactCore::PropertyType::Float, impl_->settings.particleOpacity, -219);
    opacityProp->setSoftRange(0.0, 1.0);
    opacityProp->setStep(0.01);
    particleGroup.addProperty(opacityProp);
    auto colorModeProp = makeProp(QStringLiteral("particle.colorMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.colorMode), -218);
    colorModeProp->setTooltip(QStringLiteral("0=Solid, 1=AxisGradient, 2=SourceColor"));
    particleGroup.addProperty(colorModeProp);
    particleGroup.addProperty(makeProp(QStringLiteral("particle.solidColor"), ArtifactCore::PropertyType::Color, impl_->settings.solidColor, -217));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.gradientStartColor"), ArtifactCore::PropertyType::Color, impl_->settings.gradientStartColor, -216));
    particleGroup.addProperty(makeProp(QStringLiteral("particle.gradientEndColor"), ArtifactCore::PropertyType::Color, impl_->settings.gradientEndColor, -215));
    groups.push_back(particleGroup);

    ArtifactCore::PropertyGroup fieldGroup(QStringLiteral("Field"));
    auto noiseAmountProp = makeProp(QStringLiteral("field.noiseAmount"), ArtifactCore::PropertyType::Float, impl_->settings.noiseAmount, -204);
    noiseAmountProp->setSoftRange(0.0, 100.0);
    fieldGroup.addProperty(noiseAmountProp);
    auto noiseScaleProp = makeProp(QStringLiteral("field.noiseScale"), ArtifactCore::PropertyType::Float, impl_->settings.noiseScale, -203);
    noiseScaleProp->setSoftRange(0.0, 1.0);
    fieldGroup.addProperty(noiseScaleProp);
    auto noiseSpeedProp = makeProp(QStringLiteral("field.noiseSpeed"), ArtifactCore::PropertyType::Float, impl_->settings.noiseSpeed, -202);
    noiseSpeedProp->setSoftRange(-10.0, 10.0);
    fieldGroup.addProperty(noiseSpeedProp);
    auto noisePhaseProp = makeProp(QStringLiteral("field.noisePhase"), ArtifactCore::PropertyType::Float, impl_->settings.noisePhase, -201);
    noisePhaseProp->setSoftRange(-1000.0, 1000.0);
    fieldGroup.addProperty(noisePhaseProp);
    auto twistProp = makeProp(QStringLiteral("field.twistAmount"), ArtifactCore::PropertyType::Float, impl_->settings.twistAmount, -200);
    twistProp->setSoftRange(-50.0, 50.0);
    fieldGroup.addProperty(twistProp);
    auto falloffProp = makeProp(QStringLiteral("field.falloff"), ArtifactCore::PropertyType::Float, impl_->settings.falloff, -199);
    falloffProp->setSoftRange(0.0, 5000.0);
    fieldGroup.addProperty(falloffProp);
    auto seedProp = makeProp(QStringLiteral("field.seed"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.seed), -198);
    seedProp->setHardRange(0, 2147483647);
    fieldGroup.addProperty(seedProp);
    groups.push_back(fieldGroup);

    ArtifactCore::PropertyGroup renderGroup(QStringLiteral("Render"));
    auto blendModeProp = makeProp(QStringLiteral("render.blendMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.renderSettings.blendMode), -180);
    blendModeProp->setTooltip(QStringLiteral("0=Additive, 1=Subtractive, 2=Normal, 3=Screen, 4=Multiply"));
    renderGroup.addProperty(blendModeProp);
    auto billboardModeProp = makeProp(QStringLiteral("render.billboardMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.renderSettings.billboardMode), -179);
    billboardModeProp->setTooltip(QStringLiteral("0=None, 1=ScreenAligned, 2=ViewPlane, 3=VelocityAligned"));
    renderGroup.addProperty(billboardModeProp);
    auto sortModeProp = makeProp(QStringLiteral("render.sortMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->settings.renderSettings.sortMode), -178);
    sortModeProp->setTooltip(QStringLiteral("0=None, 1=Distance, 2=OldestFirst, 3=YoungestFirst"));
    renderGroup.addProperty(sortModeProp);
    renderGroup.addProperty(makeProp(QStringLiteral("render.depthTest"), ArtifactCore::PropertyType::Boolean, impl_->settings.renderSettings.depthTest, -177));
    renderGroup.addProperty(makeProp(QStringLiteral("render.depthWrite"), ArtifactCore::PropertyType::Boolean, impl_->settings.renderSettings.depthWrite, -176));
    groups.push_back(renderGroup);

    return groups;
}

bool ArtifactFormParticleLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    auto changedAnything = [this]() {
        setDirty();
        impl_->syncSourceSize(this);
        impl_->markDirty();
        Q_EMIT changed();
        return true;
    };

    if (propertyPath == QStringLiteral("form.mode")) {
        impl_->settings.generatorMode = static_cast<FormParticleSettings::GeneratorMode>(
            clampValue(value.toInt(), 0, 2));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.columns")) {
        impl_->settings.columns = clampValue(value.toInt(), 1, 512);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.rows")) {
        impl_->settings.rows = clampValue(value.toInt(), 1, 512);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.depth")) {
        impl_->settings.depth = clampValue(value.toInt(), 1, 128);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.spacingX")) {
        impl_->settings.spacingX = std::max(0.01f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.spacingY")) {
        impl_->settings.spacingY = std::max(0.01f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.spacingZ")) {
        impl_->settings.spacingZ = std::max(0.01f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.maxParticles")) {
        impl_->settings.maxParticles = clampValue(value.toInt(), 1, 100000);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("form.originMode")) {
        impl_->settings.originMode = static_cast<FormParticleSettings::OriginMode>(
            clampValue(value.toInt(), 0, 2));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("source.path")) {
        impl_->settings.sourcePath = value.toString();
        impl_->sourceLoadAttempted = false;
        impl_->sourceLoaded = false;
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("source.alphaThreshold")) {
        impl_->settings.sourceAlphaThreshold =
            clampValue(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("source.lumaThreshold")) {
        impl_->settings.sourceLumaThreshold =
            clampValue(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.size")) {
        impl_->settings.particleSize = std::max(0.1f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.opacity")) {
        impl_->settings.particleOpacity = clampValue(static_cast<float>(value.toDouble()), 0.0f, 1.0f);
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.colorMode")) {
        impl_->settings.colorMode = static_cast<FormParticleSettings::ColorMode>(
            clampValue(value.toInt(), 0, 2));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.solidColor")) {
        impl_->settings.solidColor = value.value<QColor>();
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.gradientStartColor")) {
        impl_->settings.gradientStartColor = value.value<QColor>();
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("particle.gradientEndColor")) {
        impl_->settings.gradientEndColor = value.value<QColor>();
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.noiseAmount")) {
        impl_->settings.noiseAmount = std::max(0.0f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.noiseScale")) {
        impl_->settings.noiseScale = std::max(0.0f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.noiseSpeed")) {
        impl_->settings.noiseSpeed = static_cast<float>(value.toDouble());
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.noisePhase")) {
        impl_->settings.noisePhase = static_cast<float>(value.toDouble());
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.twistAmount")) {
        impl_->settings.twistAmount = static_cast<float>(value.toDouble());
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.falloff")) {
        impl_->settings.falloff = std::max(0.0f, static_cast<float>(value.toDouble()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("field.seed")) {
        impl_->settings.seed = static_cast<std::uint32_t>(std::max(0, value.toInt()));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("render.blendMode")) {
        impl_->settings.renderSettings.blendMode =
            static_cast<FormParticleBlendMode>(clampValue(value.toInt(), 0, 4));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("render.billboardMode")) {
        impl_->settings.renderSettings.billboardMode =
            static_cast<FormParticleBillboardMode>(clampValue(value.toInt(), 0, 3));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("render.sortMode")) {
        impl_->settings.renderSettings.sortMode =
            static_cast<FormParticleSortMode>(clampValue(value.toInt(), 0, 3));
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("render.depthTest")) {
        impl_->settings.renderSettings.depthTest = value.toBool();
        return changedAnything();
    }
    if (propertyPath == QStringLiteral("render.depthWrite")) {
        impl_->settings.renderSettings.depthWrite = value.toBool();
        return changedAnything();
    }
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

void ArtifactFormParticleLayer::applyPropertiesFromJson(const QJsonObject& obj)
{
    ArtifactAbstractLayer::fromJsonProperties(obj);
    ArtifactAbstractLayer::applyPropertiesFromJson(obj);
    if (obj.contains("formParticle") && obj.value("formParticle").isObject()) {
        impl_->settings.fromJson(obj.value("formParticle").toObject());
    } else {
        impl_->settings.fromJson(obj);
    }
    impl_->syncSourceSize(this);
    setDirty();
    impl_->markDirty();
}

void ArtifactFormParticleLayer::fromJsonProperties(const QJsonObject& obj)
{
    applyPropertiesFromJson(obj);
}

std::shared_ptr<ArtifactFormParticleLayer> createFormParticleLayer()
{
    return std::make_shared<ArtifactFormParticleLayer>();
}

std::shared_ptr<ArtifactFormParticleLayer> createFormParticleLayer(const QString& preset)
{
    auto layer = std::make_shared<ArtifactFormParticleLayer>();
    layer->loadPreset(preset);
    return layer;
}

} // namespace Artifact
