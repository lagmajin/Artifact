module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <QColor>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonValue>
#include <QMatrix4x4>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QVector2D>
#include <QVector3D>

module Artifact.Layer.Procedural3D;

import Artifact.Render.IRenderer;
import Artifact.Composition.Abstract;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Audio.Segment;
import Color.Float;
import Material.Material;
import Mesh;
import Image.ImageF32x4_RGBA;
import FloatRGBA;
import Procedural3DGenerators;
import Size;
import Time.Rational;
import Utils.Id;

namespace Artifact {

namespace {

constexpr int kSettingsVersion = 1;

QJsonObject colorToJson(const QColor& color)
{
    return {
        {QStringLiteral("r"), color.red()},
        {QStringLiteral("g"), color.green()},
        {QStringLiteral("b"), color.blue()},
        {QStringLiteral("a"), color.alpha()}
    };
}

QColor colorFromJson(const QJsonValue& value, const QColor& fallback)
{
    if (!value.isObject()) {
        return fallback;
    }
    const auto object = value.toObject();
    return QColor(
        std::clamp(object.value(QStringLiteral("r")).toInt(fallback.red()), 0, 255),
        std::clamp(object.value(QStringLiteral("g")).toInt(fallback.green()), 0, 255),
        std::clamp(object.value(QStringLiteral("b")).toInt(fallback.blue()), 0, 255),
        std::clamp(object.value(QStringLiteral("a")).toInt(fallback.alpha()), 0, 255));
}

ArtifactCore::Mesh makeMesh(const ArtifactCore::Procedural3DMeshData& generated)
{
    ArtifactCore::Mesh mesh;
    if (generated.vertices.empty() || generated.indices.size() < 3u) {
        return mesh;
    }

    mesh.setVertexCount(static_cast<int>(generated.vertices.size()));
    auto positions = mesh.vertexAttributes().add<QVector3D>("position");
    auto normals = mesh.vertexAttributes().add<QVector3D>("normal");
    auto uvs = mesh.vertexAttributes().add<QVector2D>("uv");
    for (int index = 0; index < static_cast<int>(generated.vertices.size()); ++index) {
        const auto& vertex = generated.vertices[static_cast<std::size_t>(index)];
        (*positions)[index] = QVector3D(vertex.px, vertex.py, vertex.pz);
        (*normals)[index] = QVector3D(vertex.nx, vertex.ny, vertex.nz);
        (*uvs)[index] = QVector2D(vertex.u, vertex.v);
    }

    for (std::size_t index = 0; index + 2u < generated.indices.size(); index += 3u) {
        mesh.addPolygon({
            static_cast<int>(generated.indices[index]),
            static_cast<int>(generated.indices[index + 1u]),
            static_cast<int>(generated.indices[index + 2u])
        });
    }
    mesh.updateBounds();
    return mesh;
}

QPointF cubicPoint(const QPointF& p0,
                   const QPointF& p1,
                   const QPointF& p2,
                   const QPointF& p3,
                   float t)
{
    const float oneMinusT = 1.0f - t;
    const float a = oneMinusT * oneMinusT * oneMinusT;
    const float b = 3.0f * oneMinusT * oneMinusT * t;
    const float c = 3.0f * oneMinusT * t * t;
    const float d = t * t * t;
    return QPointF(
        p0.x() * a + p1.x() * b + p2.x() * c + p3.x() * d,
        p0.y() * a + p1.y() * b + p2.y() * c + p3.y() * d);
}

} // namespace

class ArtifactProcedural3DLayer::Impl {
public:
    Procedural3DLayerKind kind = Procedural3DLayerKind::Terrain;
    ArtifactCore::TerrainSettings terrain;
    ArtifactCore::PathTubeSettings pathTube;
    ArtifactCore::Procedural3DShading shading = ArtifactCore::Procedural3DShading::Solid;
    QColor baseColor = QColor(86, 205, 179);
    float wireThickness = 1.0f;
    QString terrainHeightSourcePath;
    float terrainAudioGain = 4.0f;
    QString pathSourceLayerId;
    ArtifactCore::ImageF32x4_RGBA terrainHeightImage;
    QString loadedTerrainHeightSourcePath;
    qint64 loadedTerrainHeightModifiedMs = -1;
    ArtifactCore::Material material = ArtifactCore::Material::makeDefault();
    ArtifactCore::Mesh mesh;
    qint64 cachedFrame = -1;
    int cachedQuality = -1;
    std::uint64_t revision = 1u;
    std::uint64_t cachedRevision = 0u;

    void invalidate()
    {
        ++revision;
        cachedFrame = -1;
        cachedQuality = -1;
    }

    void refreshTerrainHeightSource()
    {
        if (kind != Procedural3DLayerKind::Terrain ||
            terrain.heightSource != ArtifactCore::TerrainHeightSource::ImageLuminance ||
            terrainHeightSourcePath.isEmpty()) {
            if (!terrain.heightSamples.empty()) {
                terrain.heightSamples.clear();
                terrain.heightSampleWidth = 0;
                terrain.heightSampleHeight = 0;
                ++revision;
            }
            return;
        }

        const QFileInfo sourceInfo(terrainHeightSourcePath);
        const qint64 modifiedMs = sourceInfo.exists()
            ? sourceInfo.lastModified().toMSecsSinceEpoch()
            : -1;
        if (loadedTerrainHeightSourcePath == terrainHeightSourcePath &&
            loadedTerrainHeightModifiedMs == modifiedMs) {
            return;
        }

        loadedTerrainHeightSourcePath = terrainHeightSourcePath;
        loadedTerrainHeightModifiedMs = modifiedMs;
        terrain.heightSamples.clear();
        terrain.heightSampleWidth = 0;
        terrain.heightSampleHeight = 0;
        if (terrainHeightImage.load(terrainHeightSourcePath) &&
            !terrainHeightImage.isEmpty()) {
            const int width = std::clamp(terrainHeightImage.width(), 1, 1024);
            const int height = std::clamp(terrainHeightImage.height(), 1, 1024);
            terrain.heightSampleWidth = width;
            terrain.heightSampleHeight = height;
            terrain.heightSamples.resize(
                static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
            for (int y = 0; y < height; ++y) {
                const int sourceY = height <= 1
                    ? 0
                    : static_cast<int>(std::lround(
                          static_cast<double>(y) *
                          static_cast<double>(terrainHeightImage.height() - 1) /
                          static_cast<double>(height - 1)));
                for (int x = 0; x < width; ++x) {
                    const int sourceX = width <= 1
                        ? 0
                        : static_cast<int>(std::lround(
                              static_cast<double>(x) *
                              static_cast<double>(terrainHeightImage.width() - 1) /
                              static_cast<double>(width - 1)));
                    const auto pixel = terrainHeightImage.getPixel(sourceX, sourceY);
                    terrain.heightSamples[
                        static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                        static_cast<std::size_t>(x)] =
                        std::clamp(
                            pixel.r() * 0.2126f +
                            pixel.g() * 0.7152f +
                            pixel.b() * 0.0722f,
                            0.0f,
                            1.0f);
                }
            }
        }
        ++revision;
    }

    void refreshPathSource(ArtifactAbstractComposition* composition, qint64 frame)
    {
        if (kind != Procedural3DLayerKind::PathTube ||
            pathTube.pathSource != ArtifactCore::ProceduralPathSource::ControlPoints ||
            !composition ||
            pathSourceLayerId.isEmpty()) {
            return;
        }

        const auto source = composition->layerById(ArtifactCore::LayerID(pathSourceLayerId));
        if (!source) {
            if (!pathTube.pathPoints.empty()) {
                pathTube.pathPoints.clear();
                ++revision;
            }
            return;
        }

        std::vector<QPointF> sampled;
        bool closed = false;
        if (source->maskCount() > 0) {
            const LayerMask layerMask = source->mask(0);
            if (layerMask.isEnabled() && layerMask.maskPathCount() > 0) {
                const MaskPath path = layerMask.maskPath(0).sampleAtFrame(frame);
                const int count = path.vertexCount();
                closed = path.isClosed();
                if (count > 0) {
                    sampled.push_back(path.vertex(0).position);
                    const int segmentCount = closed ? count : count - 1;
                    for (int segment = 0; segment < segmentCount; ++segment) {
                        const MaskVertex a = path.vertex(segment);
                        const MaskVertex b = path.vertex((segment + 1) % count);
                        const QPointF controlA = a.position + a.outTangent;
                        const QPointF controlB = b.position + b.inTangent;
                        for (int step = 1; step <= 8; ++step) {
                            sampled.push_back(cubicPoint(
                                a.position,
                                controlA,
                                controlB,
                                b.position,
                                static_cast<float>(step) / 8.0f));
                        }
                    }
                    if (closed && sampled.size() > 1u) {
                        sampled.pop_back();
                    }
                }
            }
        }

        if (sampled.size() < 2u) {
            if (const auto* shape = dynamic_cast<ArtifactShapeLayer*>(source.get())) {
                if (shape->hasCustomPath()) {
                    const auto vertices = shape->customPathVertices();
                    closed = shape->customPathClosed();
                    if (!vertices.empty()) {
                        sampled.push_back(vertices.front().pos);
                        const int count = static_cast<int>(vertices.size());
                        const int segmentCount = closed ? count : count - 1;
                        for (int segment = 0; segment < segmentCount; ++segment) {
                            const auto& a = vertices[static_cast<std::size_t>(segment)];
                            const auto& b = vertices[
                                static_cast<std::size_t>((segment + 1) % count)];
                            for (int step = 1; step <= 8; ++step) {
                                sampled.push_back(cubicPoint(
                                    a.pos,
                                    a.pos + a.outTangent,
                                    b.pos + b.inTangent,
                                    b.pos,
                                    static_cast<float>(step) / 8.0f));
                            }
                        }
                        if (closed && sampled.size() > 1u) {
                            sampled.pop_back();
                        }
                    }
                } else if (shape->hasCustomPolygon()) {
                    sampled = shape->customPolygonPoints();
                    closed = shape->customPolygonClosed();
                } else {
                    const float halfWidth = std::max(0.5f, shape->shapeWidth() * 0.5f);
                    const float halfHeight = std::max(0.5f, shape->shapeHeight() * 0.5f);
                    if (shape->shapeType() == ShapeType::Ellipse) {
                        constexpr int kEllipseSamples = 32;
                        for (int sample = 0; sample < kEllipseSamples; ++sample) {
                            const float angle =
                                static_cast<float>(sample) /
                                static_cast<float>(kEllipseSamples) *
                                6.28318530718f;
                            sampled.emplace_back(
                                std::cos(angle) * halfWidth,
                                std::sin(angle) * halfHeight);
                        }
                        closed = true;
                    } else if (shape->shapeType() == ShapeType::Line) {
                        sampled = {
                            QPointF(-halfWidth, 0.0),
                            QPointF(halfWidth, 0.0)
                        };
                        closed = false;
                    } else {
                        sampled = {
                            QPointF(-halfWidth, -halfHeight),
                            QPointF(halfWidth, -halfHeight),
                            QPointF(halfWidth, halfHeight),
                            QPointF(-halfWidth, halfHeight)
                        };
                        closed = true;
                    }
                }
            }
        }

        std::vector<std::array<float, 3>> normalizedPoints;
        if (sampled.size() >= 2u) {
            double minX = sampled.front().x();
            double maxX = minX;
            double minY = sampled.front().y();
            double maxY = minY;
            for (const auto& point : sampled) {
                minX = std::min(minX, point.x());
                maxX = std::max(maxX, point.x());
                minY = std::min(minY, point.y());
                maxY = std::max(maxY, point.y());
            }
            const double centerX = (minX + maxX) * 0.5;
            const double centerY = (minY + maxY) * 0.5;
            const double extent = std::max(1.0e-6, std::max(maxX - minX, maxY - minY));
            normalizedPoints.reserve(sampled.size());
            for (const auto& point : sampled) {
                normalizedPoints.push_back({
                    static_cast<float>((point.x() - centerX) * 2.0 / extent),
                    static_cast<float>((point.y() - centerY) * 2.0 / extent),
                    0.0f
                });
            }
        }

        if (pathTube.pathPoints != normalizedPoints ||
            pathTube.pathClosed != closed) {
            pathTube.pathPoints = std::move(normalizedPoints);
            pathTube.pathClosed = closed;
            ++revision;
        }
    }

    void refreshTerrainAudio(ArtifactAbstractComposition* composition,
                             qint64 frame,
                             float fps)
    {
        if (kind != Procedural3DLayerKind::Terrain ||
            terrain.heightSource != ArtifactCore::TerrainHeightSource::AudioAmplitude) {
            return;
        }

        constexpr int kSampleRate = 48000;
        const int sampleCount = std::max(
            1,
            static_cast<int>(std::lround(
                static_cast<double>(kSampleRate) /
                std::max(0.001, static_cast<double>(fps)))));
        ArtifactCore::AudioSegment segment;
        float amplitude = 0.0f;
        const bool audioAvailable =
            composition &&
            composition->getAudio(
                segment,
                FramePosition(frame),
                sampleCount,
                kSampleRate) &&
            segment.channelCount() > 0;
        if (audioAvailable) {
            double sumSquares = 0.0;
            std::size_t valueCount = 0u;
            for (const auto& channel : segment.channelData) {
                for (const float sample : channel) {
                    const double value = static_cast<double>(sample);
                    sumSquares += value * value;
                    ++valueCount;
                }
            }
            if (valueCount > 0u) {
                amplitude = static_cast<float>(
                    std::sqrt(sumSquares / static_cast<double>(valueCount)));
            }
        }
        amplitude = std::clamp(amplitude * terrainAudioGain, 0.0f, 1.0f);
        if (std::abs(terrain.audioAmplitude - amplitude) > 1.0e-6f ||
            terrain.audioAvailable != audioAvailable) {
            terrain.audioAmplitude = amplitude;
            terrain.audioAvailable = audioAvailable;
            ++revision;
        }
    }

    void regenerate(qint64 frame, float timeSeconds, int qualityOverride)
    {
        refreshTerrainHeightSource();
        if (cachedFrame == frame &&
            cachedRevision == revision &&
            cachedQuality == qualityOverride) {
            return;
        }
        ArtifactCore::Procedural3DMeshData generated;
        if (kind == Procedural3DLayerKind::Terrain) {
            auto resolved = terrain;
            if (qualityOverride >= 0) {
                resolved.quality = static_cast<ArtifactCore::Procedural3DQuality>(
                    std::clamp(qualityOverride, 0, 2));
            }
            generated = ArtifactCore::Procedural3DGenerators::generateTerrain(
                resolved,
                timeSeconds);
        } else {
            auto resolved = pathTube;
            if (qualityOverride >= 0) {
                resolved.quality = static_cast<ArtifactCore::Procedural3DQuality>(
                    std::clamp(qualityOverride, 0, 2));
            }
            generated = ArtifactCore::Procedural3DGenerators::generatePathTube(
                resolved,
                timeSeconds);
        }
        mesh = makeMesh(generated);
        material.setBaseColor(baseColor);
        cachedFrame = frame;
        cachedQuality = qualityOverride;
        cachedRevision = revision;
    }

    QJsonObject toJson() const
    {
        QJsonObject terrainJson {
            {QStringLiteral("seed"), static_cast<int>(terrain.seed)},
            {QStringLiteral("heightSource"), static_cast<int>(terrain.heightSource)},
            {QStringLiteral("uvMode"), static_cast<int>(terrain.uvMode)},
            {QStringLiteral("heightSourcePath"), terrainHeightSourcePath},
            {QStringLiteral("audioGain"), terrainAudioGain},
            {QStringLiteral("columns"), terrain.columns},
            {QStringLiteral("rows"), terrain.rows},
            {QStringLiteral("sizeX"), terrain.sizeX},
            {QStringLiteral("sizeY"), terrain.sizeY},
            {QStringLiteral("height"), terrain.height},
            {QStringLiteral("noiseScale"), terrain.noiseScale},
            {QStringLiteral("noiseAmplitude"), terrain.noiseAmplitude},
            {QStringLiteral("noiseOctaves"), terrain.noiseOctaves},
            {QStringLiteral("noiseEvolution"), terrain.noiseEvolution},
            {QStringLiteral("quality"), static_cast<int>(terrain.quality)}
        };
        QJsonObject pathJson {
            {QStringLiteral("seed"), static_cast<int>(pathTube.seed)},
            {QStringLiteral("pathSource"), static_cast<int>(pathTube.pathSource)},
            {QStringLiteral("pathSourceLayerId"), pathSourceLayerId},
            {QStringLiteral("profile"), static_cast<int>(pathTube.profile)},
            {QStringLiteral("pathSamples"), pathTube.pathSamples},
            {QStringLiteral("sides"), pathTube.sides},
            {QStringLiteral("radius"), pathTube.radius},
            {QStringLiteral("taperStart"), pathTube.taperStart},
            {QStringLiteral("taperEnd"), pathTube.taperEnd},
            {QStringLiteral("twist"), pathTube.twist},
            {QStringLiteral("pathOffset"), pathTube.pathOffset},
            {QStringLiteral("repeatCount"), pathTube.repeatCount},
            {QStringLiteral("pathScale"), pathTube.pathScale},
            {QStringLiteral("noiseScale"), pathTube.noiseScale},
            {QStringLiteral("noiseAmplitude"), pathTube.noiseAmplitude},
            {QStringLiteral("quality"), static_cast<int>(pathTube.quality)}
        };
        return {
            {QStringLiteral("version"), kSettingsVersion},
            {QStringLiteral("kind"), static_cast<int>(kind)},
            {QStringLiteral("shading"), static_cast<int>(shading)},
            {QStringLiteral("wireThickness"), wireThickness},
            {QStringLiteral("baseColor"), colorToJson(baseColor)},
            {QStringLiteral("terrain"), terrainJson},
            {QStringLiteral("pathTube"), pathJson}
        };
    }

    void fromJson(const QJsonObject& object)
    {
        kind = static_cast<Procedural3DLayerKind>(
            std::clamp(object.value(QStringLiteral("kind")).toInt(static_cast<int>(kind)), 0, 1));
        shading = static_cast<ArtifactCore::Procedural3DShading>(
            std::clamp(object.value(QStringLiteral("shading")).toInt(static_cast<int>(shading)), 0, 3));
        wireThickness = std::max(
            0.1f,
            static_cast<float>(
                object.value(QStringLiteral("wireThickness")).toDouble(wireThickness)));
        baseColor = colorFromJson(object.value(QStringLiteral("baseColor")), baseColor);

        const auto terrainJson = object.value(QStringLiteral("terrain")).toObject();
        terrain.seed = static_cast<std::uint32_t>(std::max(0, terrainJson.value(QStringLiteral("seed")).toInt(static_cast<int>(terrain.seed))));
        terrain.heightSource = static_cast<ArtifactCore::TerrainHeightSource>(
            std::clamp(terrainJson.value(QStringLiteral("heightSource")).toInt(static_cast<int>(terrain.heightSource)), 0, 2));
        terrain.uvMode = static_cast<ArtifactCore::TerrainUvMode>(
            std::clamp(terrainJson.value(QStringLiteral("uvMode")).toInt(static_cast<int>(terrain.uvMode)), 0, 2));
        terrainHeightSourcePath = terrainJson.value(QStringLiteral("heightSourcePath")).toString();
        terrainAudioGain = std::max(
            0.0f,
            static_cast<float>(
                terrainJson.value(QStringLiteral("audioGain")).toDouble(terrainAudioGain)));
        terrain.columns = terrainJson.value(QStringLiteral("columns")).toInt(terrain.columns);
        terrain.rows = terrainJson.value(QStringLiteral("rows")).toInt(terrain.rows);
        terrain.sizeX = static_cast<float>(terrainJson.value(QStringLiteral("sizeX")).toDouble(terrain.sizeX));
        terrain.sizeY = static_cast<float>(terrainJson.value(QStringLiteral("sizeY")).toDouble(terrain.sizeY));
        terrain.height = static_cast<float>(terrainJson.value(QStringLiteral("height")).toDouble(terrain.height));
        terrain.noiseScale = static_cast<float>(terrainJson.value(QStringLiteral("noiseScale")).toDouble(terrain.noiseScale));
        terrain.noiseAmplitude = static_cast<float>(terrainJson.value(QStringLiteral("noiseAmplitude")).toDouble(terrain.noiseAmplitude));
        terrain.noiseOctaves = terrainJson.value(QStringLiteral("noiseOctaves")).toInt(terrain.noiseOctaves);
        terrain.noiseEvolution = static_cast<float>(terrainJson.value(QStringLiteral("noiseEvolution")).toDouble(terrain.noiseEvolution));
        terrain.quality = static_cast<ArtifactCore::Procedural3DQuality>(
            std::clamp(terrainJson.value(QStringLiteral("quality")).toInt(static_cast<int>(terrain.quality)), 0, 2));

        const auto pathJson = object.value(QStringLiteral("pathTube")).toObject();
        pathTube.seed = static_cast<std::uint32_t>(std::max(0, pathJson.value(QStringLiteral("seed")).toInt(static_cast<int>(pathTube.seed))));
        pathTube.pathSource = static_cast<ArtifactCore::ProceduralPathSource>(
            std::clamp(pathJson.value(QStringLiteral("pathSource")).toInt(static_cast<int>(pathTube.pathSource)), 0, 1));
        pathSourceLayerId = pathJson.value(QStringLiteral("pathSourceLayerId")).toString();
        pathTube.profile = static_cast<ArtifactCore::ProceduralPathProfile>(
            std::clamp(pathJson.value(QStringLiteral("profile")).toInt(static_cast<int>(pathTube.profile)), 0, 1));
        pathTube.pathSamples = pathJson.value(QStringLiteral("pathSamples")).toInt(pathTube.pathSamples);
        pathTube.sides = pathJson.value(QStringLiteral("sides")).toInt(pathTube.sides);
        pathTube.radius = static_cast<float>(pathJson.value(QStringLiteral("radius")).toDouble(pathTube.radius));
        pathTube.taperStart = static_cast<float>(pathJson.value(QStringLiteral("taperStart")).toDouble(pathTube.taperStart));
        pathTube.taperEnd = static_cast<float>(pathJson.value(QStringLiteral("taperEnd")).toDouble(pathTube.taperEnd));
        pathTube.twist = static_cast<float>(pathJson.value(QStringLiteral("twist")).toDouble(pathTube.twist));
        pathTube.pathOffset = static_cast<float>(pathJson.value(QStringLiteral("pathOffset")).toDouble(pathTube.pathOffset));
        pathTube.repeatCount = static_cast<float>(pathJson.value(QStringLiteral("repeatCount")).toDouble(pathTube.repeatCount));
        pathTube.pathScale = static_cast<float>(pathJson.value(QStringLiteral("pathScale")).toDouble(pathTube.pathScale));
        pathTube.noiseScale = static_cast<float>(pathJson.value(QStringLiteral("noiseScale")).toDouble(pathTube.noiseScale));
        pathTube.noiseAmplitude = static_cast<float>(pathJson.value(QStringLiteral("noiseAmplitude")).toDouble(pathTube.noiseAmplitude));
        pathTube.quality = static_cast<ArtifactCore::Procedural3DQuality>(
            std::clamp(pathJson.value(QStringLiteral("quality")).toInt(static_cast<int>(pathTube.quality)), 0, 2));
        invalidate();
    }
};

ArtifactProcedural3DLayer::ArtifactProcedural3DLayer(Procedural3DLayerKind kind)
    : impl_(new Impl())
{
    impl_->kind = kind;
    impl_->terrain = ArtifactCore::Procedural3DGenerators::makeTerrainPreset();
    impl_->pathTube = ArtifactCore::Procedural3DGenerators::makePathTubePreset();
    impl_->baseColor = kind == Procedural3DLayerKind::Terrain
        ? QColor(86, 205, 179)
        : QColor(255, 89, 140);
    setIs3D(true);
    setSourceSize(Size_2D(1000, 1000));
}

ArtifactProcedural3DLayer::~ArtifactProcedural3DLayer()
{
    delete impl_;
}

Procedural3DLayerKind ArtifactProcedural3DLayer::generatorKind() const
{
    return impl_->kind;
}

void ArtifactProcedural3DLayer::setGeneratorKind(Procedural3DLayerKind kind)
{
    if (impl_->kind == kind) {
        return;
    }
    impl_->kind = kind;
    impl_->invalidate();
    setDirty();
    Q_EMIT changed();
}

void ArtifactProcedural3DLayer::draw(ArtifactIRenderer* renderer)
{
    drawResolved(renderer, -1);
}

void ArtifactProcedural3DLayer::drawLOD(ArtifactIRenderer* renderer, DetailLevel lod)
{
    const int quality = lod == DetailLevel::Low
        ? static_cast<int>(ArtifactCore::Procedural3DQuality::Draft)
        : (lod == DetailLevel::Medium
               ? static_cast<int>(ArtifactCore::Procedural3DQuality::Preview)
               : static_cast<int>(ArtifactCore::Procedural3DQuality::Full));
    drawResolved(renderer, quality);
}

void ArtifactProcedural3DLayer::drawResolved(ArtifactIRenderer* renderer,
                                             int qualityOverride)
{
    if (!renderer || !isVisible()) {
        return;
    }
    const qint64 frame = currentFrame();
    auto* composition = dynamic_cast<ArtifactAbstractComposition*>(compositionObject());
    const float fps = composition
        ? std::max(0.001f, static_cast<float>(composition->frameRate().framerate()))
        : 30.0f;
    const float timeSeconds = static_cast<float>(frame) / fps;
    impl_->refreshPathSource(composition, frame);
    impl_->refreshTerrainAudio(composition, frame, fps);
    impl_->regenerate(frame, timeSeconds, qualityOverride);
    if (!impl_->mesh.isValid()) {
        return;
    }

    QMatrix4x4 model;
    model.setToIdentity();
    const auto& transform = transform3D();
    const RationalTime frameTime(frame, static_cast<int64_t>(std::lround(fps)));
    const auto snapshot = transform.snapshotAt(frameTime);
    model.translate(snapshot.positionX, snapshot.positionY, snapshot.positionZ);
    model.rotate(snapshot.rotation, 0.0f, 0.0f, 1.0f);
    model.scale(snapshot.scaleX, snapshot.scaleY, 1.0f);
    model.translate(-snapshot.anchorX, -snapshot.anchorY, -snapshot.anchorZ);

    if (impl_->shading == ArtifactCore::Procedural3DShading::Wire) {
        const auto positions = impl_->mesh.vertexAttributes().get<QVector3D>("position");
        if (!positions) {
            return;
        }
        const FloatColor color {
            impl_->baseColor.redF(),
            impl_->baseColor.greenF(),
            impl_->baseColor.blueF(),
            opacity() * impl_->baseColor.alphaF()
        };
        for (int polygon = 0; polygon < impl_->mesh.polygonCount(); ++polygon) {
            const auto indices = impl_->mesh.getPolygonVertices(polygon);
            for (int edge = 0; edge < indices.size(); ++edge) {
                const auto a = model.map((*positions)[indices[edge]]);
                const auto b = model.map((*positions)[indices[(edge + 1) % indices.size()]]);
                renderer->draw3DLine({a.x(), a.y(), a.z()},
                                     {b.x(), b.y(), b.z()},
                                     color,
                                     impl_->wireThickness);
            }
        }
        return;
    }

    renderer->drawMesh(QStringLiteral("procedural3d:%1").arg(id().toString()),
                       impl_->mesh,
                       impl_->material,
                       model,
                       opacity(),
                       static_cast<int>(impl_->shading));
}

QRectF ArtifactProcedural3DLayer::localBounds() const
{
    if (impl_->kind == Procedural3DLayerKind::Terrain) {
        return QRectF(-impl_->terrain.sizeX * 0.5f,
                      -impl_->terrain.sizeY * 0.5f,
                      impl_->terrain.sizeX,
                      impl_->terrain.sizeY);
    }
    const float extent = impl_->pathTube.pathScale + impl_->pathTube.radius;
    return QRectF(-extent, -extent, extent * 2.0f, extent * 2.0f);
}

QString ArtifactProcedural3DLayer::debugState() const
{
    const QString sourceState = impl_->kind == Procedural3DLayerKind::Terrain
        ? (impl_->terrain.heightSource == ArtifactCore::TerrainHeightSource::ImageLuminance
               ? (impl_->terrain.heightSamples.empty()
                      ? QStringLiteral("image-missing")
                      : QStringLiteral("image-loaded"))
               : (impl_->terrain.heightSource == ArtifactCore::TerrainHeightSource::AudioAmplitude
                      ? (impl_->terrain.audioAvailable
                             ? QStringLiteral("audio:%1").arg(
                                   impl_->terrain.audioAmplitude,
                                   0,
                                   'f',
                                   3)
                             : QStringLiteral("audio-missing:fallback-noise"))
                      : QStringLiteral("noise")))
        : (impl_->pathTube.pathSource == ArtifactCore::ProceduralPathSource::ControlPoints
               ? (impl_->pathTube.pathPoints.size() >= 2u
                      ? QStringLiteral("layer-path-loaded")
                      : QStringLiteral("layer-path-missing"))
               : QStringLiteral("parametric"));
    return QStringLiteral("Procedural3DLayer{kind=%1 frame=%2 vertices=%3 polygons=%4 quality=%5 shading=%6 source=%7}")
        .arg(impl_->kind == Procedural3DLayerKind::Terrain
                 ? QStringLiteral("terrain")
                 : QStringLiteral("pathTube"))
        .arg(impl_->cachedFrame)
        .arg(impl_->mesh.vertexCount())
        .arg(impl_->mesh.polygonCount())
        .arg(impl_->cachedQuality >= 0
                 ? impl_->cachedQuality
                 : static_cast<int>(impl_->kind == Procedural3DLayerKind::Terrain
                                        ? impl_->terrain.quality
                                        : impl_->pathTube.quality))
        .arg(static_cast<int>(impl_->shading))
        .arg(sourceState);
}

void ArtifactProcedural3DLayer::loadPreset(const QString& presetName)
{
    const QString preset = presetName.trimmed().toLower();
    if (preset == QStringLiteral("lowpolyterrain")) {
        impl_->kind = Procedural3DLayerKind::Terrain;
        impl_->terrain = ArtifactCore::Procedural3DGenerators::makeTerrainPreset(
            1u,
            ArtifactCore::Procedural3DQuality::Draft);
        impl_->terrain.columns = 28;
        impl_->terrain.rows = 18;
        impl_->terrain.height = 2.8f;
        impl_->terrain.noiseScale = 0.7f;
        impl_->baseColor = QColor(205, 189, 126);
        impl_->shading = ArtifactCore::Procedural3DShading::Solid;
    } else if (preset == QStringLiteral("wirelandscape")) {
        impl_->kind = Procedural3DLayerKind::Terrain;
        impl_->terrain = ArtifactCore::Procedural3DGenerators::makeTerrainPreset(
            17u,
            ArtifactCore::Procedural3DQuality::Preview);
        impl_->terrain.sizeX = 14.0f;
        impl_->terrain.sizeY = 8.0f;
        impl_->terrain.height = 2.4f;
        impl_->baseColor = QColor(76, 232, 198);
        impl_->shading = ArtifactCore::Procedural3DShading::Wire;
    } else if (preset == QStringLiteral("softclothwave")) {
        impl_->kind = Procedural3DLayerKind::Terrain;
        impl_->terrain = ArtifactCore::Procedural3DGenerators::makeTerrainPreset(
            41u,
            ArtifactCore::Procedural3DQuality::Full);
        impl_->terrain.height = 0.65f;
        impl_->terrain.noiseScale = 0.38f;
        impl_->terrain.noiseOctaves = 3;
        impl_->terrain.noiseEvolution = 0.18f;
        impl_->baseColor = QColor(237, 174, 139);
        impl_->shading = ArtifactCore::Procedural3DShading::Lit;
    } else if (preset == QStringLiteral("neonpathtube")) {
        impl_->kind = Procedural3DLayerKind::PathTube;
        impl_->pathTube = ArtifactCore::Procedural3DGenerators::makePathTubePreset(
            9u,
            ArtifactCore::Procedural3DQuality::Preview);
        impl_->pathTube.profile = ArtifactCore::ProceduralPathProfile::Tube;
        impl_->pathTube.radius = 0.11f;
        impl_->pathTube.repeatCount = 1.7f;
        impl_->pathTube.noiseAmplitude = 0.12f;
        impl_->baseColor = QColor(255, 67, 149);
        impl_->shading = ArtifactCore::Procedural3DShading::Lit;
    } else if (preset == QStringLiteral("ribbontrail")) {
        impl_->kind = Procedural3DLayerKind::PathTube;
        impl_->pathTube = ArtifactCore::Procedural3DGenerators::makePathTubePreset(
            23u,
            ArtifactCore::Procedural3DQuality::Preview);
        impl_->pathTube.profile = ArtifactCore::ProceduralPathProfile::Ribbon;
        impl_->pathTube.radius = 0.28f;
        impl_->pathTube.taperEnd = 0.05f;
        impl_->pathTube.twist = 5.0f;
        impl_->pathTube.repeatCount = 0.8f;
        impl_->baseColor = QColor(255, 190, 68);
        impl_->shading = ArtifactCore::Procedural3DShading::Solid;
    } else {
        return;
    }
    impl_->invalidate();
    setDirty();
    Q_EMIT changed();
}

QStringList ArtifactProcedural3DLayer::availablePresets() const
{
    return {
        QStringLiteral("lowPolyTerrain"),
        QStringLiteral("wireLandscape"),
        QStringLiteral("softClothWave"),
        QStringLiteral("neonPathTube"),
        QStringLiteral("ribbonTrail")
    };
}

QJsonObject ArtifactProcedural3DLayer::toJson() const
{
    QJsonObject json = ArtifactAbstractLayer::toJson();
    json[QStringLiteral("type")] = static_cast<int>(LayerType::Procedural3D);
    json[QStringLiteral("layerType")] = QStringLiteral("Procedural3DLayer");
    json[QStringLiteral("isProcedural3DLayer")] = true;
    json[QStringLiteral("procedural3D")] = impl_->toJson();
    return json;
}

ArtifactAbstractLayerPtr ArtifactProcedural3DLayer::fromJson(const QJsonObject& object)
{
    auto layer = std::make_shared<ArtifactProcedural3DLayer>();
    layer->fromJsonProperties(object);
    return layer;
}

void ArtifactProcedural3DLayer::fromJsonProperties(const QJsonObject& object)
{
    ArtifactAbstractLayer::fromJsonProperties(object);
    const auto settings = object.value(QStringLiteral("procedural3D"));
    if (settings.isObject()) {
        impl_->fromJson(settings.toObject());
    }
}

std::vector<ArtifactCore::PropertyGroup> ArtifactProcedural3DLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    auto property = [this](const QString& path, ArtifactCore::PropertyType type,
                           const QVariant& value, int priority) {
        return persistentLayerProperty(path, type, value, priority);
    };

    ArtifactCore::PropertyGroup generator(QStringLiteral("Geometry"));
    generator.addProperty(property(QStringLiteral("procedural.kind"), ArtifactCore::PropertyType::Integer,
                                   static_cast<int>(impl_->kind), -250));
    if (impl_->kind == Procedural3DLayerKind::Terrain) {
        generator.addProperty(property(QStringLiteral("terrain.columns"), ArtifactCore::PropertyType::Integer, impl_->terrain.columns, -249));
        generator.addProperty(property(QStringLiteral("terrain.rows"), ArtifactCore::PropertyType::Integer, impl_->terrain.rows, -248));
        generator.addProperty(property(QStringLiteral("terrain.sizeX"), ArtifactCore::PropertyType::Float, impl_->terrain.sizeX, -247));
        generator.addProperty(property(QStringLiteral("terrain.sizeY"), ArtifactCore::PropertyType::Float, impl_->terrain.sizeY, -246));
        generator.addProperty(property(QStringLiteral("terrain.height"), ArtifactCore::PropertyType::Float, impl_->terrain.height, -245));
        generator.addProperty(property(QStringLiteral("terrain.seed"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->terrain.seed), -244));
        auto heightSourceProperty = property(
            QStringLiteral("terrain.heightSource"),
            ArtifactCore::PropertyType::Integer,
            static_cast<int>(impl_->terrain.heightSource),
            -243);
        heightSourceProperty->setTooltip(
            QStringLiteral("0=Noise, 1=Image Luminance, 2=Composition Audio"));
        generator.addProperty(heightSourceProperty);
        generator.addProperty(property(QStringLiteral("terrain.heightSourcePath"), ArtifactCore::PropertyType::String, impl_->terrainHeightSourcePath, -242));
        generator.addProperty(property(QStringLiteral("terrain.audioGain"), ArtifactCore::PropertyType::Float, impl_->terrainAudioGain, -241));
        generator.addProperty(property(QStringLiteral("terrain.uvMode"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->terrain.uvMode), -240));
    } else {
        auto pathSourceProperty = property(
            QStringLiteral("path.source"),
            ArtifactCore::PropertyType::Integer,
            static_cast<int>(impl_->pathTube.pathSource),
            -250);
        pathSourceProperty->setTooltip(
            QStringLiteral("0=Parametric, 1=Mask or Shape Layer"));
        generator.addProperty(pathSourceProperty);
        auto pathSourceLayer = property(
            QStringLiteral("path.sourceLayerId"),
            ArtifactCore::PropertyType::ObjectReference,
            impl_->pathSourceLayerId,
            -249);
        ArtifactCore::PropertyMetadata sourceMetadata;
        sourceMetadata.referenceTypeName = QStringLiteral("LayerID");
        pathSourceLayer->setMetadata(sourceMetadata);
        generator.addProperty(pathSourceLayer);
        generator.addProperty(property(QStringLiteral("path.profile"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->pathTube.profile), -249));
        generator.addProperty(property(QStringLiteral("path.samples"), ArtifactCore::PropertyType::Integer, impl_->pathTube.pathSamples, -248));
        generator.addProperty(property(QStringLiteral("path.sides"), ArtifactCore::PropertyType::Integer, impl_->pathTube.sides, -247));
        generator.addProperty(property(QStringLiteral("path.radius"), ArtifactCore::PropertyType::Float, impl_->pathTube.radius, -246));
        generator.addProperty(property(QStringLiteral("path.taperStart"), ArtifactCore::PropertyType::Float, impl_->pathTube.taperStart, -245));
        generator.addProperty(property(QStringLiteral("path.taperEnd"), ArtifactCore::PropertyType::Float, impl_->pathTube.taperEnd, -244));
        generator.addProperty(property(QStringLiteral("path.twist"), ArtifactCore::PropertyType::Float, impl_->pathTube.twist, -243));
        generator.addProperty(property(QStringLiteral("path.pathOffset"), ArtifactCore::PropertyType::Float, impl_->pathTube.pathOffset, -242));
        generator.addProperty(property(QStringLiteral("path.repeatCount"), ArtifactCore::PropertyType::Float, impl_->pathTube.repeatCount, -241));
        generator.addProperty(property(QStringLiteral("path.pathScale"), ArtifactCore::PropertyType::Float, impl_->pathTube.pathScale, -240));
        generator.addProperty(property(QStringLiteral("path.seed"), ArtifactCore::PropertyType::Integer, static_cast<int>(impl_->pathTube.seed), -239));
    }
    groups.push_back(generator);

    ArtifactCore::PropertyGroup displacement(QStringLiteral("Displacement"));
    if (impl_->kind == Procedural3DLayerKind::Terrain) {
        displacement.addProperty(property(QStringLiteral("terrain.noiseScale"), ArtifactCore::PropertyType::Float, impl_->terrain.noiseScale, -230));
        displacement.addProperty(property(QStringLiteral("terrain.noiseAmplitude"), ArtifactCore::PropertyType::Float, impl_->terrain.noiseAmplitude, -229));
        displacement.addProperty(property(QStringLiteral("terrain.noiseOctaves"), ArtifactCore::PropertyType::Integer, impl_->terrain.noiseOctaves, -228));
        displacement.addProperty(property(QStringLiteral("terrain.noiseEvolution"), ArtifactCore::PropertyType::Float, impl_->terrain.noiseEvolution, -227));
    } else {
        displacement.addProperty(property(QStringLiteral("path.noiseScale"), ArtifactCore::PropertyType::Float, impl_->pathTube.noiseScale, -230));
        displacement.addProperty(property(QStringLiteral("path.noiseAmplitude"), ArtifactCore::PropertyType::Float, impl_->pathTube.noiseAmplitude, -229));
    }
    groups.push_back(displacement);

    ArtifactCore::PropertyGroup material(QStringLiteral("Material"));
    material.addProperty(property(QStringLiteral("procedural.baseColor"), ArtifactCore::PropertyType::Color, impl_->baseColor, -210));
    auto shadingProperty = property(
        QStringLiteral("procedural.shading"),
        ArtifactCore::PropertyType::Integer,
        static_cast<int>(impl_->shading),
        -209);
    shadingProperty->setTooltip(
        QStringLiteral("0=Wire, 1=Solid, 2=Normal, 3=Lit"));
    material.addProperty(shadingProperty);
    material.addProperty(property(
        QStringLiteral("procedural.wireThickness"),
        ArtifactCore::PropertyType::Float,
        impl_->wireThickness,
        -208));
    material.addProperty(property(QStringLiteral("procedural.quality"), ArtifactCore::PropertyType::Integer,
                                  static_cast<int>(impl_->kind == Procedural3DLayerKind::Terrain
                                                       ? impl_->terrain.quality
                                                       : impl_->pathTube.quality),
                                  -208));
    groups.push_back(material);
    return groups;
}

bool ArtifactProcedural3DLayer::setLayerPropertyValue(const QString& path, const QVariant& value)
{
    auto changed = [this]() {
        impl_->invalidate();
        setDirty();
        Q_EMIT changed();
        return true;
    };
    if (path == QStringLiteral("procedural.kind")) {
        impl_->kind = static_cast<Procedural3DLayerKind>(std::clamp(value.toInt(), 0, 1));
        return changed();
    }
    if (path == QStringLiteral("procedural.baseColor")) {
        impl_->baseColor = value.value<QColor>();
        return changed();
    }
    if (path == QStringLiteral("procedural.shading")) {
        impl_->shading = static_cast<ArtifactCore::Procedural3DShading>(std::clamp(value.toInt(), 0, 3));
        return changed();
    }
    if (path == QStringLiteral("procedural.quality")) {
        const auto quality = static_cast<ArtifactCore::Procedural3DQuality>(std::clamp(value.toInt(), 0, 2));
        impl_->terrain.quality = quality;
        impl_->pathTube.quality = quality;
        return changed();
    }
    if (path == QStringLiteral("terrain.columns")) { impl_->terrain.columns = std::max(1, value.toInt()); return changed(); }
    if (path == QStringLiteral("terrain.rows")) { impl_->terrain.rows = std::max(1, value.toInt()); return changed(); }
    if (path == QStringLiteral("terrain.sizeX")) { impl_->terrain.sizeX = std::max(0.001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("terrain.sizeY")) { impl_->terrain.sizeY = std::max(0.001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("terrain.height")) { impl_->terrain.height = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("terrain.seed")) { impl_->terrain.seed = static_cast<std::uint32_t>(std::max(0, value.toInt())); return changed(); }
    if (path == QStringLiteral("terrain.heightSource")) {
        impl_->terrain.heightSource = static_cast<ArtifactCore::TerrainHeightSource>(
            std::clamp(value.toInt(), 0, 2));
        impl_->loadedTerrainHeightSourcePath.clear();
        return changed();
    }
    if (path == QStringLiteral("terrain.heightSourcePath")) {
        impl_->terrainHeightSourcePath = value.toString();
        impl_->loadedTerrainHeightSourcePath.clear();
        return changed();
    }
    if (path == QStringLiteral("procedural.wireThickness")) {
        impl_->wireThickness =
            std::max(0.1f, static_cast<float>(value.toDouble()));
        return changed();
    }
    if (path == QStringLiteral("terrain.audioGain")) {
        impl_->terrainAudioGain =
            std::max(0.0f, static_cast<float>(value.toDouble()));
        return changed();
    }
    if (path == QStringLiteral("terrain.uvMode")) {
        impl_->terrain.uvMode = static_cast<ArtifactCore::TerrainUvMode>(
            std::clamp(value.toInt(), 0, 2));
        return changed();
    }
    if (path == QStringLiteral("terrain.noiseScale")) { impl_->terrain.noiseScale = std::max(0.0001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("terrain.noiseAmplitude")) { impl_->terrain.noiseAmplitude = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("terrain.noiseOctaves")) { impl_->terrain.noiseOctaves = std::clamp(value.toInt(), 1, 12); return changed(); }
    if (path == QStringLiteral("terrain.noiseEvolution")) { impl_->terrain.noiseEvolution = static_cast<float>(value.toDouble()); return changed(); }
    if (path == QStringLiteral("path.profile")) { impl_->pathTube.profile = static_cast<ArtifactCore::ProceduralPathProfile>(std::clamp(value.toInt(), 0, 1)); return changed(); }
    if (path == QStringLiteral("path.source")) {
        impl_->pathTube.pathSource = static_cast<ArtifactCore::ProceduralPathSource>(
            std::clamp(value.toInt(), 0, 1));
        impl_->pathTube.pathPoints.clear();
        return changed();
    }
    if (path == QStringLiteral("path.sourceLayerId")) {
        impl_->pathSourceLayerId = value.toString();
        impl_->pathTube.pathPoints.clear();
        return changed();
    }
    if (path == QStringLiteral("path.samples")) { impl_->pathTube.pathSamples = std::max(2, value.toInt()); return changed(); }
    if (path == QStringLiteral("path.sides")) { impl_->pathTube.sides = std::max(3, value.toInt()); return changed(); }
    if (path == QStringLiteral("path.radius")) { impl_->pathTube.radius = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.taperStart")) { impl_->pathTube.taperStart = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.taperEnd")) { impl_->pathTube.taperEnd = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.twist")) { impl_->pathTube.twist = static_cast<float>(value.toDouble()); return changed(); }
    if (path == QStringLiteral("path.pathOffset")) { impl_->pathTube.pathOffset = static_cast<float>(value.toDouble()); return changed(); }
    if (path == QStringLiteral("path.repeatCount")) { impl_->pathTube.repeatCount = std::max(0.001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.pathScale")) { impl_->pathTube.pathScale = std::max(0.001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.seed")) { impl_->pathTube.seed = static_cast<std::uint32_t>(std::max(0, value.toInt())); return changed(); }
    if (path == QStringLiteral("path.noiseScale")) { impl_->pathTube.noiseScale = std::max(0.0001f, static_cast<float>(value.toDouble())); return changed(); }
    if (path == QStringLiteral("path.noiseAmplitude")) { impl_->pathTube.noiseAmplitude = std::max(0.0f, static_cast<float>(value.toDouble())); return changed(); }
    return ArtifactAbstractLayer::setLayerPropertyValue(path, value);
}

std::shared_ptr<ArtifactProcedural3DLayer> createTerrainLayer()
{
    return std::make_shared<ArtifactProcedural3DLayer>(Procedural3DLayerKind::Terrain);
}

std::shared_ptr<ArtifactProcedural3DLayer> createPathTubeLayer()
{
    return std::make_shared<ArtifactProcedural3DLayer>(Procedural3DLayerKind::PathTube);
}

} // namespace Artifact
