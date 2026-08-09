module;
#include <QDir>
#include <QColor>
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
#include <QtGlobal>
#include <algorithm>
#include <functional>

module Artifact.Export.RmlUiWriter;

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

QString rgba(const QJsonObject& object, const QString& prefix) {
    const int r = qBound(0, qRound(object.value(prefix + QStringLiteral("R")).toDouble(1.0) * 255.0), 255);
    const int g = qBound(0, qRound(object.value(prefix + QStringLiteral("G")).toDouble(1.0) * 255.0), 255);
    const int b = qBound(0, qRound(object.value(prefix + QStringLiteral("B")).toDouble(1.0) * 255.0), 255);
    const double a = qBound(0.0, object.value(prefix + QStringLiteral("A")).toDouble(1.0), 1.0);
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(r).arg(g).arg(b).arg(a, 0, 'f', 3);
}

QString layerTypeMarkup(const ArtifactExportLayerSnapshot& layer, const QString& assetUrl) {
    const auto& object = layer.serialized;
    if (layer.requiresPreRender && !assetUrl.isEmpty()) {
        return QStringLiteral("<div class=\"baked-layer\"></div>");
    }
    if (object.contains(QStringLiteral("text.value"))) {
        return QStringLiteral("<span class=\"layer-text\">%1</span>")
            .arg(object.value(QStringLiteral("text.value")).toString().toHtmlEscaped());
    }
    if (object.contains(QStringLiteral("image.sourcePath"))) {
        const int imageWidth = object.value(QStringLiteral("image.width")).toInt();
        const int imageHeight = object.value(QStringLiteral("image.height")).toInt();
        if (imageWidth > 0 && imageHeight > 0) {
            return QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%3\" draggable=\"false\" />")
                .arg(assetUrl).arg(imageWidth).arg(imageHeight);
        }
        return QStringLiteral("<img src=\"%1\" draggable=\"false\" />").arg(assetUrl);
    }
    if (object.contains(QStringLiteral("solidColor"))) {
        return QString();
    }
    return QString();
}

QString layerStyle(const ArtifactExportLayerSnapshot& layer,
                   const QString& assetUrl,
                   const QDir& output) {
    const auto& object = layer.serialized;
    const auto transform = object.value(QStringLiteral("transform")).toObject();
    const double x = transform.value(QStringLiteral("px")).toDouble();
    const double y = transform.value(QStringLiteral("py")).toDouble();
    const double ax = transform.value(QStringLiteral("ax")).toDouble();
    const double ay = transform.value(QStringLiteral("ay")).toDouble();
    const double sx = transform.value(QStringLiteral("sx")).toDouble(1.0);
    const double sy = transform.value(QStringLiteral("sy")).toDouble(1.0);
    const double rotation = transform.value(QStringLiteral("rx")).toDouble();
    QString style = QStringLiteral("left:%1px;top:%2px;opacity:%3;transform-origin:%4px %5px;transform:rotate(%6deg) scale(%7,%8);")
        .arg(x).arg(y).arg(layer.opacity, 0, 'f', 4).arg(ax).arg(ay).arg(rotation).arg(sx).arg(sy);
    if (layer.requiresPreRender) {
        const QSize bakedSize = QImageReader(output.filePath(assetUrl)).size();
        style += QStringLiteral("width:%1px;height:%2px;background-image:url(%3);background-size:100% 100%;")
            .arg(std::max(1, bakedSize.width()))
            .arg(std::max(1, bakedSize.height()))
            .arg(assetUrl);
        return style;
    }
    if (object.contains(QStringLiteral("shapeWidth"))) {
        style += QStringLiteral("width:%1px;height:%2px;")
            .arg(object.value(QStringLiteral("shapeWidth")).toDouble())
            .arg(object.value(QStringLiteral("shapeHeight")).toDouble());
        if (object.value(QStringLiteral("fillEnabled")).toBool(true)) {
            style += QStringLiteral("background-color:%1;").arg(rgba(object, QStringLiteral("fill")));
        }
        if (object.value(QStringLiteral("shapeType")).toInt(0) == 1) {
            style += QStringLiteral("border-radius:50%;");
        } else {
            style += QStringLiteral("border-radius:%1px;")
                .arg(object.value(QStringLiteral("cornerRadius")).toDouble());
        }
        if (object.value(QStringLiteral("strokeEnabled")).toBool(false)) {
            style += QStringLiteral("border:%1px solid %2;")
                .arg(object.value(QStringLiteral("strokeWidth")).toDouble())
                .arg(rgba(object, QStringLiteral("stroke")));
        }
    } else if (object.contains(QStringLiteral("solidColor"))) {
        const auto solid = object.value(QStringLiteral("solidColor")).toObject();
        style += QStringLiteral("width:%1px;height:%2px;background-color:rgba(%3,%4,%5,%6);")
            .arg(object.value(QStringLiteral("solidWidth")).toDouble())
            .arg(object.value(QStringLiteral("solidHeight")).toDouble())
            .arg(qRound(solid.value(QStringLiteral("r")).toDouble() * 255.0))
            .arg(qRound(solid.value(QStringLiteral("g")).toDouble() * 255.0))
            .arg(qRound(solid.value(QStringLiteral("b")).toDouble() * 255.0))
            .arg(solid.value(QStringLiteral("a")).toDouble(1.0), 0, 'f', 3);
    }
    if (object.contains(QStringLiteral("text.value"))) {
        style += QStringLiteral("font-family:'%1';font-size:%2px;")
            .arg(object.value(QStringLiteral("text.fontFamily")).toString(QStringLiteral("Arial")).replace(QLatin1Char('\''), QStringLiteral("\\'")))
            .arg(object.value(QStringLiteral("text.fontSize")).toDouble(24.0));
        const int alignment = object.value(QStringLiteral("text.alignment")).toInt(0);
        const QString textAlign = alignment == 1 ? QStringLiteral("center")
            : alignment == 2 ? QStringLiteral("right")
            : alignment == 3 ? QStringLiteral("justify")
                             : QStringLiteral("left");
        style += QStringLiteral("text-align:%1;").arg(textAlign);
        const QColor textColor(object.value(QStringLiteral("text.color")).toString());
        if (textColor.isValid()) {
            style += QStringLiteral("color:rgba(%1,%2,%3,%4);")
                .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                .arg(textColor.alphaF(), 0, 'f', 3);
        }
    }
    return style;
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

void appendTransformAnimation(QString& css,
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
    css += QStringLiteral("@keyframes %1{").arg(animationName);
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
        css += QStringLiteral("%1%{left:%2px;top:%3px;opacity:%4;transform:rotate(%5deg) scale(%6,%7);}")
            .arg(progress, 0, 'f', 3).arg(x).arg(y).arg(frameOpacity)
            .arg(angle).arg(sx).arg(sy);
    }
    const double duration = (compositionOutPoint - compositionInPoint) / std::max(1.0, frameRate);
    css += QStringLiteral("}.%1{animation:%3 %2s linear both;}")
        .arg(className).arg(duration, 0, 'f', 4).arg(animationName);
}

void appendBakedAnimation(QString& css,
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
    css += QStringLiteral("@keyframes %1{").arg(animationName);
    for (int index = 0; index < assetUrls.size(); ++index) {
        const double progress = assetUrls.size() > 1
            ? (static_cast<double>(index) / static_cast<double>(assetUrls.size() - 1)) * 100.0
            : 0.0;
        css += QStringLiteral("%1%{background-image:url(%2);}")
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
        css += QStringLiteral("}@keyframes %1{0%{opacity:0;}%2%{opacity:%4;}%3%{opacity:%4;}100%{opacity:0;}")
            .arg(timingName).arg(startPercent, 0, 'f', 3)
            .arg(endPercent, 0, 'f', 3).arg(layerOpacity, 0, 'f', 4);
        css += QStringLiteral("}.%1{animation:%2 %3s steps(%4) both,%5 %6s steps(1,end) both;animation-delay:%7s,0s;}")
            .arg(className).arg(animationName).arg(duration, 0, 'f', 4)
            .arg(assetUrls.size()).arg(timingName)
            .arg(compositionDuration, 0, 'f', 4).arg(delay, 0, 'f', 4);
    } else {
        css += QStringLiteral("}.%1{animation:%2 %3s steps(%4) both;animation-delay:%5s;}")
            .arg(className).arg(animationName).arg(duration, 0, 'f', 4)
            .arg(assetUrls.size()).arg(delay, 0, 'f', 4);
    }
}

bool writeText(const QString& path, const QString& value) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << value;
    return stream.status() == QTextStream::Ok;
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

} // namespace

bool ArtifactExportRmlUiWriter::write(const ArtifactCompositionPtr& composition,
                                      const QString& outputDirectory,
                                      const ArtifactRmlUiExportOptions& options,
                                      QString* errorMessage) {
    ArtifactExportSession session(composition);
    if (!session.build(errorMessage)) return false;
    QDir output(outputDirectory);
    if (!output.exists() && !QDir().mkpath(outputDirectory)) {
        if (errorMessage) *errorMessage = QStringLiteral("RmlUi出力先を作成できませんでした。");
        return false;
    }
    output.mkpath(QStringLiteral("assets"));
    if (!session.copyAssets(outputDirectory, errorMessage)) return false;
    const auto& source = session.snapshot();
    QHash<QString, QVector<int>> children;
    for (int i = 0; i < source.layers.size(); ++i) {
        children[source.layers.at(i).parentId].push_back(i);
    }

    QString markup = QStringLiteral("<rml>\n<body id=\"composition\" class=\"composition\">\n");
    QString css = QStringLiteral("body.composition{position:relative;width:%1px;height:%2px;overflow:hidden;}\n")
        .arg(source.size.width()).arg(source.size.height());
    css += QStringLiteral("body.composition{background-color:rgba(%1,%2,%3,%4);}\n")
        .arg(source.backgroundColor.red()).arg(source.backgroundColor.green())
        .arg(source.backgroundColor.blue()).arg(source.backgroundColor.alphaF(), 0, 'f', 3);
    bool appendOk = true;
    QString appendError;
    std::function<void(const QVector<int>&, int)> append = [&](const QVector<int>& indices, int depth) {
        for (const int index : indices) {
            const auto& layer = source.layers.at(index);
            if (!layer.visible) continue;
            const QString id = safeId(layer.id);
            const QString className = QStringLiteral("layer_%1").arg(index);
            QStringList bakedAssetUrls;
            if (!ensureBakedSequence(session, layer, output, options.preRenderScale,
                                     &bakedAssetUrls, &appendError)) {
                appendOk = false;
                continue;
            }
            QString assetUrl = bakedAssetUrls.isEmpty()
                ? QString()
                : bakedAssetUrls.front();
            const QString sourcePath = layer.serialized.value(QStringLiteral("image.sourcePath")).toString();
            if (!sourcePath.isEmpty() && !layer.requiresPreRender) {
                assetUrl = session.assetPathFor(sourcePath);
            }
            markup += QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("<div id=\"%1\" class=\"%2\">%3\n")
                .arg(id, className, layerTypeMarkup(layer, assetUrl));
            css += QStringLiteral(".%1{%2}\n").arg(className, layerStyle(layer, assetUrl, output));
            appendTransformAnimation(css, layer, className, source.inPoint,
                                     source.outPoint, source.frameRate);
            appendBakedAnimation(css, className, bakedAssetUrls,
                                 source.inPoint, source.outPoint, source.frameRate,
                                 layer.inPoint, layer.outPoint, layer.opacity);
            append(children.value(layer.id), depth + 1);
            markup += QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("</div>\n");
        }
    };
    append(children.value(QString()), 1);
    if (!appendOk) {
        if (errorMessage) *errorMessage = appendError;
        return false;
    }
    markup += QStringLiteral("</body>\n</rml>\n");
    if (!writeText(output.filePath(QStringLiteral("composition.rml")), markup) ||
        !writeText(output.filePath(QStringLiteral("composition.rcss")), css)) {
        if (errorMessage) *errorMessage = QStringLiteral("RmlUiファイルを書き出せませんでした。");
        return false;
    }
    return true;
}

bool ArtifactExportRmlUiWriter::write(const ArtifactCompositionPtr& composition,
                                      const QString& outputDirectory,
                                      QString* errorMessage) {
    return ArtifactExportRmlUiWriter::write(
        composition, outputDirectory, ArtifactRmlUiExportOptions{}, errorMessage);
}

} // namespace Artifact
