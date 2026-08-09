module;
#include <QDir>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <functional>

module Artifact.Export.GamefaceWriter;

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

QString writeStyle(const ArtifactExportLayerSnapshot& layer) {
    const auto object = layer.serialized;
    const auto transform = object.value(QStringLiteral("transform")).toObject();
    const double anchorX = transform.value(QStringLiteral("ax")).toDouble();
    const double anchorY = transform.value(QStringLiteral("ay")).toDouble();
    QString result = QStringLiteral("left:%1px;top:%2px;opacity:%3;transform:rotate(%4deg) scale(%5,%6);")
        .arg(transform.value(QStringLiteral("px")).toDouble())
        .arg(transform.value(QStringLiteral("py")).toDouble())
        .arg(layer.opacity, 0, 'f', 4)
        .arg(transform.value(QStringLiteral("rx")).toDouble())
        .arg(transform.value(QStringLiteral("sx")).toDouble(1.0))
        .arg(transform.value(QStringLiteral("sy")).toDouble(1.0));
    result.prepend(QStringLiteral("transform-origin:%1px %2px;").arg(anchorX).arg(anchorY));
    if (layer.requiresPreRender) return result;
    if (object.contains(QStringLiteral("shapeWidth"))) {
        result += QStringLiteral("width:%1px;height:%2px;")
            .arg(object.value(QStringLiteral("shapeWidth")).toDouble())
            .arg(object.value(QStringLiteral("shapeHeight")).toDouble());
        if (object.value(QStringLiteral("fillEnabled")).toBool(true)) {
            result += QStringLiteral("background:rgba(%1,%2,%3,%4);")
                .arg(qRound(object.value(QStringLiteral("fillR")).toDouble(1.0) * 255.0))
                .arg(qRound(object.value(QStringLiteral("fillG")).toDouble(1.0) * 255.0))
                .arg(qRound(object.value(QStringLiteral("fillB")).toDouble(1.0) * 255.0))
                .arg(object.value(QStringLiteral("fillA")).toDouble(1.0), 0, 'f', 3);
        } else {
            result += QStringLiteral("background:transparent;");
        }
        result += object.value(QStringLiteral("shapeType")).toInt(0) == 1
            ? QStringLiteral("border-radius:50%;")
            : QStringLiteral("border-radius:%1px;")
                  .arg(object.value(QStringLiteral("cornerRadius")).toDouble());
    } else if (object.contains(QStringLiteral("solidColor"))) {
        const auto color = object.value(QStringLiteral("solidColor")).toObject();
        result += QStringLiteral("width:%1px;height:%2px;background:rgba(%3,%4,%5,%6);")
            .arg(object.value(QStringLiteral("solidWidth")).toDouble())
            .arg(object.value(QStringLiteral("solidHeight")).toDouble())
            .arg(qRound(color.value(QStringLiteral("r")).toDouble() * 255.0))
            .arg(qRound(color.value(QStringLiteral("g")).toDouble() * 255.0))
            .arg(qRound(color.value(QStringLiteral("b")).toDouble() * 255.0))
            .arg(color.value(QStringLiteral("a")).toDouble(1.0), 0, 'f', 3);
    } else if (object.contains(QStringLiteral("image.sourcePath"))) {
        const int imageWidth = object.value(QStringLiteral("image.width")).toInt();
        const int imageHeight = object.value(QStringLiteral("image.height")).toInt();
        if (imageWidth > 0 && imageHeight > 0) {
            result += QStringLiteral("width:%1px;height:%2px;")
                .arg(imageWidth).arg(imageHeight);
        }
    }
    if (object.contains(QStringLiteral("text.value"))) {
        result += QStringLiteral("font-family:'%1';font-size:%2px;")
            .arg(object.value(QStringLiteral("text.fontFamily")).toString(QStringLiteral("Arial")).replace(QLatin1Char('\''), QStringLiteral("\\'")))
            .arg(object.value(QStringLiteral("text.fontSize")).toDouble(24.0));
        const int alignment = object.value(QStringLiteral("text.alignment")).toInt(0);
        const QString textAlign = alignment == 1 ? QStringLiteral("center")
            : alignment == 2 ? QStringLiteral("right")
            : alignment == 3 ? QStringLiteral("justify")
                             : QStringLiteral("left");
        result += QStringLiteral("text-align:%1;").arg(textAlign);
        const QColor textColor(object.value(QStringLiteral("text.color")).toString());
        if (textColor.isValid()) {
            result += QStringLiteral("color:rgba(%1,%2,%3,%4);")
                .arg(textColor.red()).arg(textColor.green()).arg(textColor.blue())
                .arg(textColor.alphaF(), 0, 'f', 3);
        }
    }
    return result;
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

void appendTransformAnimation(QString& animationCss,
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
    animationCss += QStringLiteral("@keyframes %1{").arg(animationName);
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
        animationCss += QStringLiteral("%1%{left:%2px;top:%3px;opacity:%4;transform:rotate(%5deg) scale(%6,%7);}")
            .arg(progress, 0, 'f', 3).arg(x).arg(y).arg(frameOpacity)
            .arg(angle).arg(sx).arg(sy);
    }
    const double duration = (compositionOutPoint - compositionInPoint) / std::max(1.0, frameRate);
    animationCss += QStringLiteral("}.%1{animation:%3 %2s linear both;}")
        .arg(className).arg(duration, 0, 'f', 4).arg(animationName);
}

bool writeText(const QString& path, const QString& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream stream(&file);
    stream << content;
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

bool ArtifactExportGamefaceWriter::write(const ArtifactCompositionPtr& composition,
                                         const QString& outputDirectory,
                                         const ArtifactGamefaceExportOptions& options,
                                         QString* errorMessage) {
    ArtifactExportSession session(composition);
    if (!session.build(errorMessage)) return false;
    QDir output(outputDirectory);
    if (!output.exists() && !QDir().mkpath(outputDirectory)) {
        if (errorMessage) *errorMessage = QStringLiteral("Gameface出力先を作成できませんでした。");
        return false;
    }
    output.mkpath(QStringLiteral("assets"));
    if (!session.copyAssets(outputDirectory, errorMessage)) return false;
    const auto& source = session.snapshot();
    QHash<QString, QVector<int>> children;
    for (int i = 0; i < source.layers.size(); ++i) children[source.layers.at(i).parentId].push_back(i);

    QString html = QStringLiteral("<!doctype html>\n<html><head><meta charset=\"utf-8\"><link rel=\"stylesheet\" href=\"styles.css\"></head><body>\n<div id=\"composition\" class=\"composition\">\n");
    QString css = QStringLiteral(".composition{position:relative;width:%1px;height:%2px;overflow:hidden;}\n.layer{position:absolute;transform-origin:0 0;}\n")
        .arg(source.size.width()).arg(source.size.height());
    QString animationsCss;
    css += QStringLiteral(".composition{background-color:rgba(%1,%2,%3,%4);}\n")
        .arg(source.backgroundColor.red()).arg(source.backgroundColor.green())
        .arg(source.backgroundColor.blue()).arg(source.backgroundColor.alphaF(), 0, 'f', 3);
    bool appendOk = true;
    QString appendError;
    QString bakedSequences;
    std::function<void(const QVector<int>&, int)> append = [&](const QVector<int>& indices, int depth) {
        for (const int index : indices) {
            const auto& layer = source.layers.at(index);
            if (!layer.visible) continue;
            const QString id = safeId(layer.id);
            const QString cls = QStringLiteral("layer_%1").arg(index);
            const auto object = layer.serialized;
            QString content;
            const QString imagePath = object.value(QStringLiteral("image.sourcePath")).toString();
            QStringList bakedAssetUrls;
            if (!ensureBakedSequence(session, layer, output, options.preRenderScale,
                                     &bakedAssetUrls, &appendError)) {
                appendOk = false;
                continue;
            }
            if (layer.requiresPreRender) {
                const QString elementId = QStringLiteral("baked_%1").arg(index);
                if (bakedAssetUrls.isEmpty()) {
                    appendOk = false;
                    appendError = QStringLiteral("ベイク画像列が空です: %1").arg(layer.name);
                    continue;
                }
                content = QStringLiteral("<img id=\"%1\" src=\"%2\" draggable=\"false\">")
                    .arg(elementId, bakedAssetUrls.front());
                QStringList quotedPaths;
                for (const auto& path : bakedAssetUrls) {
                    quotedPaths.push_back(QStringLiteral("\"%1\"").arg(path));
                }
                bakedSequences += QStringLiteral("  { elementId: \"%1\", startFrame: %2, endFrame: %3, opacity: %4, frames: [%5] },\n")
                    .arg(elementId).arg(layer.inPoint).arg(layer.outPoint)
                    .arg(layer.opacity, 0, 'f', 4)
                    .arg(quotedPaths.join(QStringLiteral(",")));
            } else if (object.contains(QStringLiteral("text.value"))) {
                content = QStringLiteral("<span>%1</span>").arg(object.value(QStringLiteral("text.value")).toString().toHtmlEscaped());
            } else if (!imagePath.isEmpty()) {
                const QString assetUrl = session.assetPathFor(imagePath);
                content = QStringLiteral("<img src=\"%1\" draggable=\"false\">").arg(assetUrl);
            }
            html += QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("<div id=\"%1\" class=\"layer %2\">%3\n").arg(id, cls, content);
            css += QStringLiteral(".%1{%2}\n").arg(cls, writeStyle(layer));
            appendTransformAnimation(animationsCss, layer, cls, source.inPoint,
                                     source.outPoint, source.frameRate);
            append(children.value(layer.id), depth + 1);
            html += QString(depth * 2, QLatin1Char(' ')) + QStringLiteral("</div>\n");
        }
    };
    append(children.value(QString()), 1);
    if (!appendOk) {
        if (errorMessage) *errorMessage = appendError;
        return false;
    }
    html = html.replace(QStringLiteral("<link rel=\"stylesheet\" href=\"styles.css\">"),
                        QStringLiteral("<link rel=\"stylesheet\" href=\"styles.css\"><link rel=\"stylesheet\" href=\"animations.css\">"));
    html += QStringLiteral("</div>\n<script src=\"app.js\"></script></body></html>\n");
    const QString js = QStringLiteral(
        "// ArtifactStudio Gameface export\n"
        "window.ArtifactComposition = { frameRate: %1, inPoint: %2, outPoint: %3 };\n"
        "const ArtifactBakedSequences = [\n%4];\n"
        "const artifactExportStart = performance.now();\n"
        "function artifactUpdateBakedSequences(now) {\n"
        "  const frame = Math.floor((now - artifactExportStart) / 1000 * window.ArtifactComposition.frameRate) + window.ArtifactComposition.inPoint;\n"
        "  for (const sequence of ArtifactBakedSequences) {\n"
        "    const image = document.getElementById(sequence.elementId);\n"
        "    if (!image || sequence.frames.length === 0) continue;\n"
        "    const active = frame >= sequence.startFrame && frame < sequence.endFrame;\n"
        "    image.style.opacity = active ? String(sequence.opacity) : \"0\";\n"
        "    if (active) {\n"
        "      const index = Math.max(0, Math.min(sequence.frames.length - 1, frame - sequence.startFrame));\n"
        "      image.src = sequence.frames[index];\n"
        "    }\n"
        "  }\n"
        "  requestAnimationFrame(artifactUpdateBakedSequences);\n"
        "}\n"
        "if (ArtifactBakedSequences.length) requestAnimationFrame(artifactUpdateBakedSequences);\n")
        .arg(source.frameRate).arg(source.inPoint).arg(source.outPoint).arg(bakedSequences);
    if (!writeText(output.filePath(QStringLiteral("index.html")), html) ||
        !writeText(output.filePath(QStringLiteral("styles.css")), css) ||
        !writeText(output.filePath(QStringLiteral("animations.css")), animationsCss) ||
        !writeText(output.filePath(QStringLiteral("app.js")), js)) {
        if (errorMessage) *errorMessage = QStringLiteral("Gamefaceファイルを書き出せませんでした。");
        return false;
    }
    return true;
}

bool ArtifactExportGamefaceWriter::write(const ArtifactCompositionPtr& composition,
                                         const QString& outputDirectory,
                                         QString* errorMessage) {
    return ArtifactExportGamefaceWriter::write(
        composition, outputDirectory, ArtifactGamefaceExportOptions{}, errorMessage);
}

} // namespace Artifact
