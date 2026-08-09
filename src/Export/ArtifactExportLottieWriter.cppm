module;
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QSize>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <any>
#include <vector>
#include <utility>
#include <initializer_list>

module Artifact.Export.LottieWriter;

import Artifact.Export.Session;
import Artifact.Export.PreRenderPipeline;
import Export.Lottie.Types;
import Export.Lottie.Exporter;
import Utils.Id;

namespace Artifact {
namespace {

using ArtifactCore::Export::Lottie::LottieDocument;
using ArtifactCore::Export::Lottie::LottieImageAsset;
using ArtifactCore::Export::Lottie::LottieLayer;
using ArtifactCore::Export::Lottie::LottiePoint;
using ArtifactCore::Export::Lottie::LottieShapeEllipse;
using ArtifactCore::Export::Lottie::LottieShapeFill;
using ArtifactCore::Export::Lottie::LottieShapePath;
using ArtifactCore::Export::Lottie::LottieShapeRect;
using ArtifactCore::Export::Lottie::LottieShapeStar;
using ArtifactCore::Export::Lottie::LottieShapeStroke;
using ArtifactCore::Export::Lottie::LottieKeyframe;

LottiePoint point(std::initializer_list<double> values) {
    LottiePoint result;
    result.k.assign(values.begin(), values.end());
    return result;
}

LottiePoint scalar(double value) {
    return point({value});
}

void addTransform(LottieLayer& target, const QJsonObject& serialized,
                  double opacity, double minFrame, double maxFrame) {
    const QJsonObject transform = serialized.value(QStringLiteral("transform")).toObject();
    target.anchor = point({transform.value(QStringLiteral("ax")).toDouble(),
                           transform.value(QStringLiteral("ay")).toDouble(), 0.0});
    target.position = point({transform.value(QStringLiteral("px")).toDouble(),
                             transform.value(QStringLiteral("py")).toDouble(), 0.0});
    target.scale = point({transform.value(QStringLiteral("sx")).toDouble(1.0) * 100.0,
                          transform.value(QStringLiteral("sy")).toDouble(1.0) * 100.0,
                          100.0});
    target.rotation = scalar(transform.value(QStringLiteral("rx")).toDouble());
    target.opacity = scalar(std::clamp(opacity, 0.0, 1.0) * 100.0);

    const QJsonArray positionKeyframes =
        transform.value(QStringLiteral("positionKeyframes")).toArray();
    for (const auto& value : positionKeyframes) {
        const auto object = value.toObject();
        LottieKeyframe frame;
        frame.t = object.value(QStringLiteral("frame")).toDouble();
        if (frame.t < minFrame || frame.t > maxFrame) continue;
        frame.s = {object.value(QStringLiteral("x")).toDouble(),
                   object.value(QStringLiteral("y")).toDouble(), 0.0};
        frame.e = frame.s;
        target.position.keyframes.push_back(std::move(frame));
    }
    const QJsonArray rotationKeyframes =
        transform.value(QStringLiteral("rotationKeyframes")).toArray();
    for (const auto& value : rotationKeyframes) {
        const auto object = value.toObject();
        LottieKeyframe frame;
        frame.t = object.value(QStringLiteral("frame")).toDouble();
        if (frame.t < minFrame || frame.t > maxFrame) continue;
        frame.s = {object.value(QStringLiteral("value")).toDouble()};
        frame.e = frame.s;
        target.rotation.keyframes.push_back(std::move(frame));
    }
    const QJsonArray scaleKeyframes =
        transform.value(QStringLiteral("scaleKeyframes")).toArray();
    for (const auto& value : scaleKeyframes) {
        const auto object = value.toObject();
        LottieKeyframe frame;
        frame.t = object.value(QStringLiteral("frame")).toDouble();
        if (frame.t < minFrame || frame.t > maxFrame) continue;
        frame.s = {object.value(QStringLiteral("x")).toDouble() * 100.0,
                   object.value(QStringLiteral("y")).toDouble() * 100.0,
                   100.0};
        frame.e = frame.s;
        target.scale.keyframes.push_back(std::move(frame));
    }
    const auto sortKeyframes = [](std::vector<LottieKeyframe>& keyframes) {
        std::stable_sort(keyframes.begin(), keyframes.end(),
                         [](const LottieKeyframe& left, const LottieKeyframe& right) {
                             return left.t < right.t;
                         });
    };
    const auto ensureInitialKeyframe = [](LottiePoint& pointValue, double frame) {
        if (pointValue.keyframes.empty() || pointValue.keyframes.front().t <= frame) return;
        LottieKeyframe initial;
        initial.t = frame;
        initial.s = pointValue.k;
        initial.e = pointValue.k;
        pointValue.keyframes.insert(pointValue.keyframes.begin(), std::move(initial));
    };
    sortKeyframes(target.position.keyframes);
    sortKeyframes(target.rotation.keyframes);
    sortKeyframes(target.scale.keyframes);
    ensureInitialKeyframe(target.position, minFrame);
    ensureInitialKeyframe(target.rotation, minFrame);
    ensureInitialKeyframe(target.scale, minFrame);
    for (std::size_t index = 0; index + 1 < target.position.keyframes.size(); ++index) {
        target.position.keyframes[index].e = target.position.keyframes[index + 1].s;
    }
    for (std::size_t index = 0; index + 1 < target.rotation.keyframes.size(); ++index) {
        target.rotation.keyframes[index].e = target.rotation.keyframes[index + 1].s;
    }
    for (std::size_t index = 0; index + 1 < target.scale.keyframes.size(); ++index) {
        target.scale.keyframes[index].e = target.scale.keyframes[index + 1].s;
    }
}

void addShapeStyle(LottieLayer& target, const QJsonObject& serialized) {
    if (serialized.value(QStringLiteral("fillEnabled")).toBool(true)) {
        LottieShapeFill fill;
        fill.c.k = {serialized.value(QStringLiteral("fillR")).toDouble(1.0),
                    serialized.value(QStringLiteral("fillG")).toDouble(1.0),
                    serialized.value(QStringLiteral("fillB")).toDouble(1.0),
                    serialized.value(QStringLiteral("fillA")).toDouble(1.0)};
        fill.o = scalar(serialized.value(QStringLiteral("fillA")).toDouble(1.0) * 100.0);
        target.shapes.emplace_back(std::move(fill));
    }
    if (serialized.value(QStringLiteral("strokeEnabled")).toBool(false)) {
        LottieShapeStroke stroke;
        stroke.c.k = {serialized.value(QStringLiteral("strokeR")).toDouble(),
                      serialized.value(QStringLiteral("strokeG")).toDouble(),
                      serialized.value(QStringLiteral("strokeB")).toDouble(),
                      serialized.value(QStringLiteral("strokeA")).toDouble(1.0)};
        stroke.o = scalar(serialized.value(QStringLiteral("strokeA")).toDouble(1.0) * 100.0);
        stroke.w = scalar(serialized.value(QStringLiteral("strokeWidth")).toDouble());
        stroke.lineCap = serialized.value(QStringLiteral("strokeCap")).toInt(1);
        stroke.lineJoin = serialized.value(QStringLiteral("strokeJoin")).toInt(1);
        target.shapes.emplace_back(std::move(stroke));
    }
}

void addShape(LottieLayer& target, const QJsonObject& serialized) {
    const double width = serialized.value(QStringLiteral("shapeWidth")).toDouble();
    const double height = serialized.value(QStringLiteral("shapeHeight")).toDouble();
    const double centerX = width * 0.5;
    const double centerY = height * 0.5;
    const auto shapeType = serialized.value(QStringLiteral("shapeType")).toInt();

    const QJsonArray customPath = serialized.value(QStringLiteral("customPath")).toArray();
    if (!customPath.isEmpty()) {
        LottieShapePath path;
        path.closed = serialized.value(QStringLiteral("customPathClosed")).toBool(true);
        for (const auto& value : customPath) {
            const auto vertex = value.toObject();
            path.vertices.push_back(vertex.value(QStringLiteral("px")).toDouble());
            path.vertices.push_back(vertex.value(QStringLiteral("py")).toDouble());
            path.inTangents.push_back(vertex.value(QStringLiteral("ix")).toDouble());
            path.inTangents.push_back(vertex.value(QStringLiteral("iy")).toDouble());
            path.outTangents.push_back(vertex.value(QStringLiteral("ox")).toDouble());
            path.outTangents.push_back(vertex.value(QStringLiteral("oy")).toDouble());
        }
        target.shapes.emplace_back(std::move(path));
    } else if (shapeType == 1) {
        LottieShapeEllipse ellipse;
        ellipse.p = point({centerX, centerY});
        ellipse.s = point({width, height});
        target.shapes.emplace_back(std::move(ellipse));
    } else if (shapeType == 2 || shapeType == 3 || shapeType == 4 || shapeType == 5) {
        LottieShapeStar star;
        star.position = point({centerX, centerY});
        star.points = scalar(shapeType == 2 ? serialized.value(QStringLiteral("starPoints")).toDouble(5.0)
                                           : shapeType == 3 ? serialized.value(QStringLiteral("polygonSides")).toDouble(6.0)
                                           : shapeType == 5 ? 3.0 : 2.0);
        star.outerRadius = scalar(std::min(width, height) * 0.5);
        star.innerRadius = scalar(star.outerRadius.k.front() *
                                  serialized.value(QStringLiteral("starInnerRadius")).toDouble(0.5));
        star.starType = shapeType == 2 ? 2 : 1;
        target.shapes.emplace_back(std::move(star));
    } else {
        LottieShapeRect rect;
        rect.p = point({centerX, centerY});
        rect.s = point({width, height});
        rect.r = scalar(serialized.value(QStringLiteral("cornerRadius")).toDouble());
        target.shapes.emplace_back(std::move(rect));
    }
    addShapeStyle(target, serialized);
}

int layerType(const ArtifactExportLayerSnapshot& source) {
    const auto& object = source.serialized;
    if (object.contains(QStringLiteral("text.value"))) return 5;
    if (object.contains(QStringLiteral("image.sourcePath"))) return 2;
    if (object.contains(QStringLiteral("solidColor"))) return 1;
    if (object.value(QStringLiteral("layerType")).toString() == QStringLiteral("Shape")) return 4;
    return source.type == 1 ? 1 : 4;
}

} // namespace

bool ArtifactExportLottieWriter::write(const ArtifactCompositionPtr& composition,
                                       const QString& outputPath,
                                       const ArtifactLottieExportOptions& options,
                                       QString* errorMessage) {
    if (outputPath.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("出力先が指定されていません。");
        return false;
    }
    ArtifactExportSession session(composition);
    if (!session.build(errorMessage)) return false;

    const auto& source = session.snapshot();
    LottieDocument document;
    document.fr = source.frameRate;
    document.ip = source.inPoint;
    document.op = source.outPoint;
    document.w = source.size.width();
    document.h = source.size.height();
    document.nm = source.name.toStdString();

    std::unordered_map<std::string, int> layerIndices;
    int index = 1;
    for (const auto& layer : source.layers) {
        layerIndices[layer.id.toStdString()] = index++;
    }

    const QDir outputDirectory = QFileInfo(outputPath).absoluteDir();
    if (!QDir().mkpath(outputDirectory.absolutePath()) ||
        !outputDirectory.mkpath(QStringLiteral("assets"))) {
        if (errorMessage) *errorMessage = QStringLiteral("Lottie出力先を作成できませんでした。");
        return false;
    }
    if (!options.embedImages && !session.copyAssets(outputDirectory.absolutePath(), errorMessage)) {
        return false;
    }
    int nextExtraLayerIndex = source.layers.size() + 1;
    const auto appendImageAsset = [&](const QString& path, const QString& assetId) {
        if (options.embedImages) {
            const auto asset = ArtifactCore::Export::Lottie::LottieExporter::makeEmbeddedImageAsset(
                path, assetId);
            if (!asset) return false;
            document.assets.emplace_back(*asset);
            return true;
        }
        LottieImageAsset asset;
        asset.id = assetId.toStdString();
        asset.fileName = QFileInfo(path).fileName().toStdString();
        asset.directory = QStringLiteral("assets/").toStdString();
        const QSize imageSize = QImageReader(path).size();
        asset.width = std::max(1, imageSize.width());
        asset.height = std::max(1, imageSize.height());
        document.assets.emplace_back(std::move(asset));
        return true;
    };

    for (const auto& layerSource : source.layers) {
        LottieLayer layer;
        layer.ind = layerIndices[layerSource.id.toStdString()];
        layer.ty = layerType(layerSource);
        layer.nm = layerSource.name.toStdString();
        layer.ip = std::clamp(std::max(source.inPoint, layerSource.inPoint),
                              source.inPoint, source.outPoint - 1);
        layer.op = std::min(source.outPoint, std::max(layer.ip + 1, layerSource.outPoint));
        layer.hidden = layerSource.visible ? 0 : 1;
        layer.blendMode = std::clamp(layerSource.blendMode, 0, 16);
        if (const auto parent = layerIndices.find(layerSource.parentId.toStdString());
            parent != layerIndices.end()) {
            layer.parent = parent->second;
        }
        addTransform(layer, layerSource.serialized, layerSource.opacity,
                     static_cast<double>(layer.ip), static_cast<double>(layer.op));

        if (layerSource.requiresPreRender) {
            const auto sourceLayer = composition->layerById(ArtifactCore::LayerID(layerSource.id));
            QString bakeError;
            ArtifactExportPreRenderSequenceOptions sequenceOptions;
            sequenceOptions.startFrame = layer.ip;
            sequenceOptions.endFrame = std::max(layer.ip, layer.op - 1);
            sequenceOptions.resolutionScale = options.preRenderScale;
            QVector<QString> bakedPaths;
            const QString bakedStem = QStringLiteral("baked_%1").arg(layer.ind);
            if (!ArtifactExportPreRenderPipeline::renderLayerSequence(
                    sourceLayer.get(), outputDirectory.filePath(QStringLiteral("assets")),
                    bakedStem, sequenceOptions, &bakedPaths, &bakeError)) {
                if (errorMessage) {
                    *errorMessage = bakeError.isEmpty()
                        ? QStringLiteral("レイヤーをプリレンダーできませんでした: %1").arg(layerSource.name)
                        : bakeError;
                }
                return false;
            }
            for (int frameIndex = 0; frameIndex < bakedPaths.size(); ++frameIndex) {
                const int frame = sequenceOptions.startFrame + frameIndex * sequenceOptions.frameStep;
                const QString bakedAssetId = QStringLiteral("%1_%2").arg(bakedStem).arg(frame, 6, 10, QLatin1Char('0'));
                if (!appendImageAsset(bakedPaths.at(frameIndex), bakedAssetId)) {
                    if (errorMessage) *errorMessage = QStringLiteral("プリレンダー画像をLottieへ登録できませんでした。");
                    return false;
                }
                LottieLayer bakedLayer = layer;
                bakedLayer.ind = frameIndex == 0 ? layer.ind : nextExtraLayerIndex++;
                bakedLayer.ty = 2;
                bakedLayer.refId = bakedAssetId.toStdString();
                bakedLayer.ip = std::clamp(frame, source.inPoint, source.outPoint - 1);
                bakedLayer.op = std::min(source.outPoint, bakedLayer.ip + 1);
                bakedLayer.nm = QStringLiteral("%1 [%2]").arg(layerSource.name).arg(frame).toStdString();
                document.layers.emplace_back(std::move(bakedLayer));
            }
            continue;
        }

        if (layer.ty == 1) {
            layer.solidWidth = layerSource.serialized.value(QStringLiteral("solidWidth")).toInt(source.size.width());
            layer.solidHeight = layerSource.serialized.value(QStringLiteral("solidHeight")).toInt(source.size.height());
            const auto solid = layerSource.serialized.value(QStringLiteral("solidColor")).toObject();
            layer.solidColor = {solid.value(QStringLiteral("r")).toDouble(1.0),
                                solid.value(QStringLiteral("g")).toDouble(1.0),
                                solid.value(QStringLiteral("b")).toDouble(1.0),
                                solid.value(QStringLiteral("a")).toDouble(1.0)};
        } else if (layer.ty == 2) {
            const QString path = layerSource.serialized.value(QStringLiteral("image.sourcePath")).toString();
            if (path.isEmpty()) {
                layer.ty = 4;
            } else {
                const QString assetId = QStringLiteral("image_%1").arg(layer.ind);
                if (options.embedImages) {
                    const auto asset = ArtifactCore::Export::Lottie::LottieExporter::makeEmbeddedImageAsset(path, assetId);
                    if (asset) {
                        layer.refId = asset->id;
                        document.assets.emplace_back(*asset);
                    } else if (errorMessage) {
                        *errorMessage = QStringLiteral("画像を埋め込めませんでした: %1").arg(path);
                        return false;
                    }
                } else {
                    const QString assetUrl = session.assetPathFor(path);
                    LottieImageAsset asset;
                    asset.id = assetId.toStdString();
                    asset.fileName = QFileInfo(assetUrl).fileName().toStdString();
                    asset.directory = QStringLiteral("assets/").toStdString();
                    const QSize decodedSize = QImageReader(path).size();
                    const int serializedWidth = layerSource.serialized.value(QStringLiteral("image.width")).toInt();
                    const int serializedHeight = layerSource.serialized.value(QStringLiteral("image.height")).toInt();
                    asset.width = std::max(1, serializedWidth > 0 ? serializedWidth : decodedSize.width());
                    asset.height = std::max(1, serializedHeight > 0 ? serializedHeight : decodedSize.height());
                    layer.refId = asset.id;
                    document.assets.emplace_back(std::move(asset));
                }
            }
        } else if (layer.ty == 4) {
            addShape(layer, layerSource.serialized);
        } else if (layer.ty == 5) {
            layer.text = layerSource.serialized.value(QStringLiteral("text.value")).toString().toStdString();
            layer.textFont = layerSource.serialized.value(QStringLiteral("text.fontFamily")).toString(QStringLiteral("Arial")).toStdString();
            layer.textFontSize = layerSource.serialized.value(QStringLiteral("text.fontSize")).toDouble(24.0);
            const QColor textColor(layerSource.serialized.value(QStringLiteral("text.color")).toString());
            if (textColor.isValid()) {
                layer.textColor = {textColor.redF(), textColor.greenF(), textColor.blueF()};
            }
            layer.textAlignment = std::clamp(
                layerSource.serialized.value(QStringLiteral("text.alignment")).toInt(0), 0, 3);
        }
        document.layers.emplace_back(std::move(layer));
    }

    ArtifactCore::Export::Lottie::LottieExportOptions coreOptions;
    coreOptions.prettyPrint = options.prettyPrint;
    coreOptions.embedImages = options.embedImages;
    coreOptions.compressKeyframes = options.compressKeyframes;
    if (!ArtifactCore::Export::Lottie::LottieExporter::exportToFile(
            document, outputPath, coreOptions)) {
        if (errorMessage) *errorMessage = QStringLiteral("Lottie JSONを書き出せませんでした。");
        return false;
    }
    return true;
}

} // namespace Artifact
