module;
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QSize>
#include <QTextStream>
#include <QVector>
#include <QUuid>
#include <QtGlobal>
#include <algorithm>
#include <functional>

module Artifact.Export.UnityUxmlWriter;

import Artifact.Export.Session;
import Artifact.Export.PreRenderPipeline;
import Utils.Id;

namespace Artifact {
namespace {

QString safeId(QString value) {
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
    if (value.isEmpty()) value = QStringLiteral("layer");
    if (!((value.at(0) >= QLatin1Char('A') && value.at(0) <= QLatin1Char('Z')) ||
          (value.at(0) >= QLatin1Char('a') && value.at(0) <= QLatin1Char('z')) ||
          value.at(0) == QLatin1Char('_'))) {
        value.prepend(QStringLiteral("layer_"));
    }
    return value;
}

QString color(const QJsonObject& object) {
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(qRound(object.value(QStringLiteral("fillR")).toDouble(1.0) * 255.0))
        .arg(qRound(object.value(QStringLiteral("fillG")).toDouble(1.0) * 255.0))
        .arg(qRound(object.value(QStringLiteral("fillB")).toDouble(1.0) * 255.0))
        .arg(object.value(QStringLiteral("fillA")).toDouble(1.0), 0, 'f', 3);
}

bool writeText(const QString& path, const QString& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << content;
    return stream.status() == QTextStream::Ok;
}

bool writeAssetMeta(const QDir& output, const QString& assetUrl) {
    if (assetUrl.isEmpty()) return false;
    const QString meta = QStringLiteral("fileFormatVersion: 2\nguid: %1\nTextureImporter:\n  serializedVersion: 12\n")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'));
    return writeText(output.filePath(assetUrl + QStringLiteral(".meta")), meta);
}

bool ensureBakedSequence(const ArtifactExportSession& session,
                         const ArtifactExportLayerSnapshot& layer,
                         const QDir& output,
                         double preRenderScale,
                         QStringList* assetUrls,
                         QString* errorMessage) {
    if (!layer.requiresPreRender) return true;
    const auto sourceLayer = session.composition()->layerById(ArtifactCore::LayerID(layer.id));
    ArtifactExportPreRenderSequenceOptions options;
    options.startFrame = layer.inPoint;
    options.endFrame = std::max(layer.inPoint, layer.outPoint - 1);
    options.resolutionScale = preRenderScale;
    QVector<QString> absolutePaths;
    if (!ArtifactExportPreRenderPipeline::renderLayerSequence(
            sourceLayer.get(), output.filePath(QStringLiteral("assets")),
            QStringLiteral("baked_%1").arg(safeId(layer.id)), options,
            &absolutePaths, errorMessage)) {
        return false;
    }
    if (assetUrls) {
        for (const auto& path : absolutePaths) {
            assetUrls->push_back(QStringLiteral("assets/%1").arg(QFileInfo(path).fileName()));
        }
    }
    return true;
}

QJsonObject keyframeAt(const QJsonArray& keyframes, int frame) {
    QJsonObject result;
    int bestFrame = 0;
    bool found = false;
    for (const auto& value : keyframes) {
        const auto object = value.toObject();
        const int candidateFrame = object.value(QStringLiteral("frame")).toInt();
        if (candidateFrame <= frame && (!found || candidateFrame >= bestFrame)) {
            result = object;
            bestFrame = candidateFrame;
            found = true;
        }
    }
    return result;
}

void appendTransformAnimation(QString& uss,
                              const ArtifactExportLayerSnapshot& layer,
                              const QString& className,
                              int compositionInPoint,
                              int compositionOutPoint,
                              double frameRate) {
    if (layer.requiresPreRender) return;
    const auto transform = layer.serialized.value(QStringLiteral("transform")).toObject();
    const auto positions = transform.value(QStringLiteral("positionKeyframes")).toArray();
    const auto rotations = transform.value(QStringLiteral("rotationKeyframes")).toArray();
    const auto scales = transform.value(QStringLiteral("scaleKeyframes")).toArray();
    QVector<int> frames;
    for (const auto& value : positions) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    for (const auto& value : rotations) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    for (const auto& value : scales) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    const bool hasLayerTiming = layer.inPoint > compositionInPoint || layer.outPoint < compositionOutPoint;
    if (frames.isEmpty() && !hasLayerTiming) return;
    frames.push_back(compositionInPoint);
    if (hasLayerTiming) {
        frames.push_back(layer.inPoint);
        frames.push_back(layer.outPoint);
        frames.push_back(compositionOutPoint);
    }
    std::sort(frames.begin(), frames.end());
    frames.erase(std::unique(frames.begin(), frames.end()), frames.end());

    const double baseX = transform.value(QStringLiteral("px")).toDouble();
    const double baseY = transform.value(QStringLiteral("py")).toDouble();
    const double baseRotation = transform.value(QStringLiteral("rx")).toDouble();
    const double baseSx = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double baseSy = transform.value(QStringLiteral("sy")).toDouble(1.0);
    const QString animationName = QStringLiteral("artifact_%1_transform").arg(className);
    uss += QStringLiteral("@keyframes %1{").arg(animationName);
    for (const int frame : frames) {
        const auto position = keyframeAt(positions, frame);
        const auto rotation = keyframeAt(rotations, frame);
        const auto scale = keyframeAt(scales, frame);
        const double x = position.isEmpty() ? baseX : position.value(QStringLiteral("x")).toDouble();
        const double y = position.isEmpty() ? baseY : position.value(QStringLiteral("y")).toDouble();
        const double angle = rotation.isEmpty() ? baseRotation : rotation.value(QStringLiteral("value")).toDouble();
        const double sx = scale.isEmpty() ? baseSx : scale.value(QStringLiteral("x")).toDouble(1.0);
        const double sy = scale.isEmpty() ? baseSy : scale.value(QStringLiteral("y")).toDouble(1.0);
        const double progress = compositionOutPoint > compositionInPoint
            ? qBound(0.0, (static_cast<double>(frame - compositionInPoint) /
                           static_cast<double>(compositionOutPoint - compositionInPoint)) * 100.0, 100.0)
            : 0.0;
        const double frameOpacity = frame >= layer.inPoint && frame < layer.outPoint
            ? layer.opacity
            : 0.0;
        uss += QStringLiteral("%1%{left:%2px;top:%3px;opacity:%4;rotate:%5deg;scale:%6 %7;}")
            .arg(progress, 0, 'f', 3).arg(x).arg(y).arg(frameOpacity)
            .arg(angle).arg(sx).arg(sy);
    }
    const double duration = (compositionOutPoint - compositionInPoint) / std::max(1.0, frameRate);
    uss += QStringLiteral("}.%1{animation-name:%2;animation-duration:%3s;animation-timing-function:linear;animation-iteration-count:1;animation-fill-mode:both;}")
        .arg(className).arg(animationName).arg(duration, 0, 'f', 4);
}

void appendBakedAnimation(QString& uss,
                          const QString& className,
                          const QStringList& assetUrls,
                          int compositionInPoint,
                          int compositionOutPoint,
                          double frameRate,
                          int layerInPoint,
                          int layerOutPoint,
                          double layerOpacity) {
    if (assetUrls.isEmpty()) return;
    const QString animationName = QStringLiteral("artifact_%1_baked").arg(className);
    uss += QStringLiteral("@keyframes %1{").arg(animationName);
    for (int index = 0; index < assetUrls.size(); ++index) {
        const double progress = assetUrls.size() > 1
            ? (static_cast<double>(index) /
               static_cast<double>(assetUrls.size() - 1)) * 100.0
            : 0.0;
        uss += QStringLiteral("%1%{background-image:url(\"%2\");}")
            .arg(progress, 0, 'f', 3).arg(assetUrls.at(index));
    }
    const double duration = (layerOutPoint - layerInPoint) /
                            std::max(1.0, frameRate);
    const double compositionDuration = (compositionOutPoint - compositionInPoint) /
                                       std::max(1.0, frameRate);
    const double delay = (layerInPoint - compositionInPoint) /
                         std::max(1.0, frameRate);
    const bool hasTiming = layerInPoint > compositionInPoint || layerOutPoint < compositionOutPoint;
    const QString timingName = QStringLiteral("artifact_%1_visibility").arg(className);
    if (hasTiming) {
        const double startPercent = qBound(0.0,
            (static_cast<double>(layerInPoint - compositionInPoint) /
             std::max(1.0, static_cast<double>(compositionOutPoint - compositionInPoint))) * 100.0,
            100.0);
        const double endPercent = qBound(startPercent,
            (static_cast<double>(layerOutPoint - compositionInPoint) /
             std::max(1.0, static_cast<double>(compositionOutPoint - compositionInPoint))) * 100.0,
            100.0);
        uss += QStringLiteral("}@keyframes %1{0%{opacity:0;}%2%{opacity:%4;}%3%{opacity:%4;}100%{opacity:0;}")
            .arg(timingName).arg(startPercent, 0, 'f', 3)
            .arg(endPercent, 0, 'f', 3).arg(layerOpacity, 0, 'f', 4);
        uss += QStringLiteral("}.%1{animation-name:%2,%3;animation-duration:%4s,%5s;animation-delay:%6s,0s;animation-timing-function:steps(%7),steps(1,end);animation-iteration-count:1,1;animation-fill-mode:both,both;}")
            .arg(className).arg(animationName).arg(timingName)
            .arg(duration, 0, 'f', 4).arg(compositionDuration, 0, 'f', 4)
            .arg(delay, 0, 'f', 4).arg(assetUrls.size());
    } else {
        uss += QStringLiteral("}.%1{animation-name:%2;animation-duration:%3s;animation-delay:%4s;animation-timing-function:steps(%5);animation-iteration-count:1;animation-fill-mode:both;}")
            .arg(className).arg(animationName).arg(duration, 0, 'f', 4)
            .arg(delay, 0, 'f', 4).arg(assetUrls.size());
    }
}

} // namespace

bool ArtifactExportUnityUxmlWriter::write(const ArtifactCompositionPtr& composition,
                                          const QString& outputDirectory,
                                          const ArtifactUnityUxmlExportOptions& options,
                                          QString* errorMessage) {
    ArtifactExportSession session(composition);
    if (!session.build(errorMessage)) return false;
    QDir output(outputDirectory);
    if (!output.exists() && !QDir().mkpath(outputDirectory)) {
        if (errorMessage) *errorMessage = QStringLiteral("Unity UI Toolkit出力先を作成できませんでした。");
        return false;
    }
    output.mkpath(QStringLiteral("assets"));
    if (!session.copyAssets(outputDirectory, errorMessage)) return false;
    const auto& source = session.snapshot();
    QHash<QString, QVector<int>> children;
    for (int i = 0; i < source.layers.size(); ++i) children[source.layers.at(i).parentId].push_back(i);

    QString uxml = QStringLiteral("<ui:UXML xmlns:ui=\"UnityEngine.UIElements\">\n<ui:VisualElement name=\"composition\" class=\"composition\">\n");
    QString uss = QStringLiteral(".composition{position:absolute;width:%1px;height:%2px;overflow:hidden;}\n.layer{position:absolute;}\n")
        .arg(source.size.width()).arg(source.size.height());
    uss += QStringLiteral(".composition{background-color:rgba(%1,%2,%3,%4);}\n")
        .arg(source.backgroundColor.red()).arg(source.backgroundColor.green())
        .arg(source.backgroundColor.blue()).arg(source.backgroundColor.alphaF(), 0, 'f', 3);
    bool appendOk = true;
    QString appendError;
    std::function<void(const QVector<int>&, int)> append = [&](const QVector<int>& indices, int depth) {
        for (const int index : indices) {
            const auto& layer = source.layers.at(index);
            if (!layer.visible) continue;
            const auto object = layer.serialized;
            const QString id = safeId(layer.id);
            const QString cls = QStringLiteral("layer_%1").arg(index);
            QString element;
            QString inner;
            QStringList bakedAssetUrls;
            if (!ensureBakedSequence(session, layer, output, options.preRenderScale,
                                     &bakedAssetUrls, &appendError)) {
                appendOk = false;
                continue;
            }
            const QString bakedAssetUrl = bakedAssetUrls.isEmpty()
                ? QString()
                : bakedAssetUrls.front();
            if (layer.requiresPreRender) {
                element = QStringLiteral("<ui:VisualElement name=\"%1\" class=\"layer %2\">").arg(id, cls);
                const QSize bakedSize = QImageReader(output.filePath(bakedAssetUrl)).size();
                uss += QStringLiteral(".%1{width:%2px;height:%3px;background-image:url(\"%4\");}\n")
                    .arg(cls).arg(std::max(1, bakedSize.width()))
                    .arg(std::max(1, bakedSize.height())).arg(bakedAssetUrl);
                for (const auto& assetUrl : bakedAssetUrls) {
                    if (!writeAssetMeta(output, assetUrl)) {
                        appendOk = false;
                        appendError = QStringLiteral("Unity画像アセットの.metaを書き出せませんでした: %1").arg(assetUrl);
                        break;
                    }
                }
                if (!appendOk) {
                    continue;
                }
            } else if (object.contains(QStringLiteral("text.value"))) {
                element = QStringLiteral("<ui:VisualElement name=\"%1\" class=\"layer %2\">").arg(id, cls);
                inner = QStringLiteral("<ui:Label text=\"%1\" />")
                    .arg(object.value(QStringLiteral("text.value")).toString().toHtmlEscaped());
            } else if (object.contains(QStringLiteral("image.sourcePath"))) {
                const QString path = object.value(QStringLiteral("image.sourcePath")).toString();
                const QString assetUrl = session.assetPathFor(path);
                const QString target = output.filePath(assetUrl);
                if (QFileInfo::exists(target) && !writeAssetMeta(output, assetUrl)) {
                    appendOk = false;
                    appendError = QStringLiteral("Unity画像アセットの.metaを書き出せませんでした: %1").arg(assetUrl);
                    continue;
                }
                element = QStringLiteral("<ui:VisualElement name=\"%1\" class=\"layer %2\">").arg(id, cls);
                const QSize decodedSize = QImageReader(target).size();
                const int imageWidth = object.value(QStringLiteral("image.width")).toInt(
                    std::max(1, decodedSize.width()));
                const int imageHeight = object.value(QStringLiteral("image.height")).toInt(
                    std::max(1, decodedSize.height()));
                uss += QStringLiteral(".%1{width:%2px;height:%3px;background-image:url(\"%4\");}\n")
                    .arg(cls).arg(std::max(1, imageWidth)).arg(std::max(1, imageHeight))
                    .arg(assetUrl);
            } else {
                element = QStringLiteral("<ui:VisualElement name=\"%1\" class=\"layer %2\">").arg(id, cls);
            }
            uxml += QString(depth * 2, QLatin1Char(' ')) + element + QStringLiteral("\n");
            if (!inner.isEmpty()) {
                uxml += QString((depth + 1) * 2, QLatin1Char(' ')) + inner + QStringLiteral("\n");
            }
            const auto transform = object.value(QStringLiteral("transform")).toObject();
            uss += QStringLiteral(".%1{left:%2px;top:%3px;opacity:%4;rotate:%5deg;scale:%6 %7;}")
                .arg(cls)
                .arg(transform.value(QStringLiteral("px")).toDouble())
                .arg(transform.value(QStringLiteral("py")).toDouble())
                .arg(layer.opacity, 0, 'f', 4)
                .arg(transform.value(QStringLiteral("rx")).toDouble())
                .arg(transform.value(QStringLiteral("sx")).toDouble(1.0))
                .arg(transform.value(QStringLiteral("sy")).toDouble(1.0));
            if (!layer.requiresPreRender && object.contains(QStringLiteral("shapeWidth"))) {
                const QString radius = object.value(QStringLiteral("shapeType")).toInt(0) == 1
                    ? QStringLiteral("50%")
                    : QStringLiteral("%1px").arg(object.value(QStringLiteral("cornerRadius")).toDouble());
                const QString fillColor = object.value(QStringLiteral("fillEnabled")).toBool(true)
                    ? color(object)
                    : QStringLiteral("transparent");
                uss += QStringLiteral("width:%1px;height:%2px;background-color:%3;border-radius:%4;")
                    .arg(object.value(QStringLiteral("shapeWidth")).toDouble())
                    .arg(object.value(QStringLiteral("shapeHeight")).toDouble())
                    .arg(fillColor)
                    .arg(radius);
            } else if (!layer.requiresPreRender && object.contains(QStringLiteral("solidColor"))) {
                const auto solid = object.value(QStringLiteral("solidColor")).toObject();
                uss += QStringLiteral("width:%1px;height:%2px;background-color:rgba(%3,%4,%5,%6);")
                    .arg(object.value(QStringLiteral("solidWidth")).toDouble())
                    .arg(object.value(QStringLiteral("solidHeight")).toDouble())
                    .arg(qRound(solid.value(QStringLiteral("r")).toDouble() * 255.0))
                    .arg(qRound(solid.value(QStringLiteral("g")).toDouble() * 255.0))
                    .arg(qRound(solid.value(QStringLiteral("b")).toDouble() * 255.0))
                    .arg(solid.value(QStringLiteral("a")).toDouble(1.0), 0, 'f', 3);
            }
            if (!layer.requiresPreRender && object.contains(QStringLiteral("text.value"))) {
                const QColor textColor(object.value(QStringLiteral("text.color")).toString());
                if (textColor.isValid()) {
                    uss += QStringLiteral("color:rgba(%1,%2,%3,%4);")
                        .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                        .arg(textColor.alphaF(), 0, 'f', 3);
                }
                uss += QStringLiteral("font-size:%1px;")
                    .arg(object.value(QStringLiteral("text.fontSize")).toDouble(24.0));
                const int alignment = object.value(QStringLiteral("text.alignment")).toInt(0);
                const QString unityTextAlign = alignment == 1 ? QStringLiteral("upper-center")
                    : alignment == 2 ? QStringLiteral("upper-right")
                    : alignment == 3 ? QStringLiteral("upper-left")
                                     : QStringLiteral("upper-left");
                uss += QStringLiteral("-unity-text-align:%1;").arg(unityTextAlign);
            }
            appendTransformAnimation(uss, layer, cls, source.inPoint,
                                     source.outPoint, source.frameRate);
            appendBakedAnimation(uss, cls, bakedAssetUrls,
                                 source.inPoint, source.outPoint, source.frameRate,
                                 layer.inPoint, layer.outPoint, layer.opacity);
            uss += QStringLiteral("\n");
            append(children.value(layer.id), depth + 1);
            uxml += QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("</ui:VisualElement>\n");
        }
    };
    append(children.value(QString()), 1);
    if (!appendOk) {
        if (errorMessage) *errorMessage = appendError;
        return false;
    }
    uxml += QStringLiteral("</ui:VisualElement>\n</ui:UXML>\n");
    const QString meta = QStringLiteral("fileFormatVersion: 2\nguid: %1\n\n")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'));
    const QString ussMeta = QStringLiteral("fileFormatVersion: 2\nguid: %1\n\n")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-'));
    if (!writeText(output.filePath(QStringLiteral("composition.uxml")), uxml) ||
        !writeText(output.filePath(QStringLiteral("composition.uss")), uss) ||
        !writeText(output.filePath(QStringLiteral("composition.uxml.meta")), meta) ||
        !writeText(output.filePath(QStringLiteral("composition.uss.meta")), ussMeta)) {
        if (errorMessage) *errorMessage = QStringLiteral("Unity UI Toolkitファイルを書き出せませんでした。");
        return false;
    }
    return true;
}

bool ArtifactExportUnityUxmlWriter::write(const ArtifactCompositionPtr& composition,
                                          const QString& outputDirectory,
                                          QString* errorMessage) {
    return ArtifactExportUnityUxmlWriter::write(
        composition, outputDirectory, ArtifactUnityUxmlExportOptions{}, errorMessage);
}

} // namespace Artifact
