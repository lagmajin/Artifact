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
#include <QTextStream>
#include <QVector>
#include <QtGlobal>
#include <algorithm>
#include <functional>

module Artifact.Export.NoesisXamlWriter;

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

QString xmlColor(const QJsonObject& object, const QString& prefix) {
    return QStringLiteral("#%1%2%3%4")
        .arg(qRound(object.value(prefix + QStringLiteral("A")).toDouble(1.0) * 255.0), 2, 16, QLatin1Char('0'))
        .arg(qRound(object.value(prefix + QStringLiteral("R")).toDouble(1.0) * 255.0), 2, 16, QLatin1Char('0'))
        .arg(qRound(object.value(prefix + QStringLiteral("G")).toDouble(1.0) * 255.0), 2, 16, QLatin1Char('0'))
        .arg(qRound(object.value(prefix + QStringLiteral("B")).toDouble(1.0) * 255.0), 2, 16, QLatin1Char('0'))
        .toUpper();
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

bool appendStoryboard(QString& styles,
                      const ArtifactExportLayerSnapshot& layer,
                      const QString& targetName,
                      int compositionInPoint,
                      int compositionOutPoint,
                      double frameRate) {
    if (layer.requiresPreRender) return false;
    const auto transform = layer.serialized.value(QStringLiteral("transform")).toObject();
    const auto positions = transform.value(QStringLiteral("positionKeyframes")).toArray();
    const auto rotations = transform.value(QStringLiteral("rotationKeyframes")).toArray();
    const auto scales = transform.value(QStringLiteral("scaleKeyframes")).toArray();
    QVector<int> frames;
    for (const auto& value : positions) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    for (const auto& value : rotations) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    for (const auto& value : scales) frames.push_back(value.toObject().value(QStringLiteral("frame")).toInt());
    const bool hasLayerTiming = layer.inPoint > compositionInPoint || layer.outPoint < compositionOutPoint;
    if (frames.isEmpty() && !hasLayerTiming) return false;
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
    const QString animationName = QStringLiteral("artifact_anim_%1").arg(targetName);
    styles += QStringLiteral("  <Storyboard x:Key=\"%1\">\n").arg(animationName);

    const auto appendAnimation = [&](const QString& target,
                                     const QString& property,
                                     const QJsonArray& keyframes,
                                     const QString& xKey,
                                     const QString& yKey,
                                     double fallbackX,
                                     double fallbackY,
                                     bool twoDimensional) {
        styles += QStringLiteral("    <DoubleAnimationUsingKeyFrames Storyboard.TargetName=\"%1\" Storyboard.TargetProperty=\"%2\">\n")
            .arg(target, property);
        for (const int frame : frames) {
            const auto object = keyframeAt(keyframes, frame);
            const double x = object.isEmpty() ? fallbackX : object.value(xKey).toDouble(fallbackX);
            const double value = twoDimensional
                ? x
                : (object.isEmpty() ? fallbackX : object.value(xKey).toDouble(fallbackX));
            const double y = object.isEmpty() ? fallbackY : object.value(yKey).toDouble(fallbackY);
            const double seconds = compositionOutPoint > compositionInPoint
                ? std::max(0.0, static_cast<double>(frame - compositionInPoint) /
                                   std::max(1.0, frameRate))
                : 0.0;
            if (twoDimensional) {
                styles += QStringLiteral("      <LinearDoubleKeyFrame KeyTime=\"0:0:%1\" Value=\"%2\" />\n")
                    .arg(seconds, 0, 'f', 6).arg(value);
            } else {
                styles += QStringLiteral("      <LinearDoubleKeyFrame KeyTime=\"0:0:%1\" Value=\"%2\" />\n")
                    .arg(seconds, 0, 'f', 6).arg(value);
            }
            Q_UNUSED(y);
        }
        styles += QStringLiteral("    </DoubleAnimationUsingKeyFrames>\n");
    };

    appendAnimation(targetName, QStringLiteral("(Canvas.Left)"), positions,
                    QStringLiteral("x"), QStringLiteral("y"), baseX, baseY, true);
    appendAnimation(targetName, QStringLiteral("(Canvas.Top)"), positions,
                    QStringLiteral("y"), QStringLiteral("x"), baseY, baseX, true);
    appendAnimation(targetName + QStringLiteral("_rotate"), QStringLiteral("Angle"), rotations,
                    QStringLiteral("value"), QString(), baseRotation, 0.0, false);
    appendAnimation(targetName + QStringLiteral("_scale"), QStringLiteral("ScaleX"), scales,
                    QStringLiteral("x"), QStringLiteral("y"), baseSx, baseSy, false);
    appendAnimation(targetName + QStringLiteral("_scale"), QStringLiteral("ScaleY"), scales,
                    QStringLiteral("y"), QStringLiteral("x"), baseSy, baseSx, false);
    styles += QStringLiteral("    <DoubleAnimationUsingKeyFrames Storyboard.TargetName=\"%1\" Storyboard.TargetProperty=\"Opacity\">\n")
        .arg(targetName);
    for (const int frame : frames) {
        const double opacity = frame >= layer.inPoint && frame < layer.outPoint
            ? layer.opacity
            : 0.0;
        const double seconds = compositionOutPoint > compositionInPoint
            ? std::max(0.0, static_cast<double>(frame - compositionInPoint) /
                               std::max(1.0, frameRate))
            : 0.0;
        styles += QStringLiteral("      <LinearDoubleKeyFrame KeyTime=\"0:0:%1\" Value=\"%2\" />\n")
            .arg(seconds, 0, 'f', 6).arg(opacity);
    }
    styles += QStringLiteral("    </DoubleAnimationUsingKeyFrames>\n");
    styles += QStringLiteral("  </Storyboard>\n");
    return true;
}

bool appendBakedStoryboard(QString& styles,
                           const QString& imageName,
                           const QStringList& assetUrls,
                           int compositionInPoint,
                           int compositionOutPoint,
                           double frameRate,
                           int layerInPoint,
                           int layerOutPoint,
                           double layerOpacity) {
    if (assetUrls.isEmpty()) return false;
    const QString animationName = QStringLiteral("artifact_baked_%1").arg(imageName);
    styles += QStringLiteral("  <Storyboard x:Key=\"%1\">\n")
        .arg(animationName);
    styles += QStringLiteral("    <ObjectAnimationUsingKeyFrames Storyboard.TargetName=\"%1\" Storyboard.TargetProperty=\"Source\">\n")
        .arg(imageName);
    for (int index = 0; index < assetUrls.size(); ++index) {
        const double seconds = compositionOutPoint > compositionInPoint
            ? static_cast<double>(layerInPoint - compositionInPoint + index) /
              std::max(1.0, frameRate)
            : 0.0;
        styles += QStringLiteral("      <DiscreteObjectKeyFrame KeyTime=\"0:0:%1\" Value=\"%2\" />\n")
            .arg(seconds, 0, 'f', 6).arg(assetUrls.at(index));
    }
    styles += QStringLiteral("    </ObjectAnimationUsingKeyFrames>\n");
    if (layerInPoint > compositionInPoint || layerOutPoint < compositionOutPoint) {
        styles += QStringLiteral("    <DoubleAnimationUsingKeyFrames Storyboard.TargetName=\"%1\" Storyboard.TargetProperty=\"Opacity\">\n")
            .arg(imageName);
        const auto appendOpacityKey = [&](int frame, double opacity) {
            const double seconds = compositionOutPoint > compositionInPoint
                ? std::max(0.0, static_cast<double>(frame - compositionInPoint) /
                                   std::max(1.0, frameRate))
                : 0.0;
            styles += QStringLiteral("      <LinearDoubleKeyFrame KeyTime=\"0:0:%1\" Value=\"%2\" />\n")
                .arg(seconds, 0, 'f', 6).arg(opacity);
        };
        appendOpacityKey(compositionInPoint, 0.0);
        appendOpacityKey(layerInPoint, layerOpacity);
        appendOpacityKey(layerOutPoint, 0.0);
        styles += QStringLiteral("    </DoubleAnimationUsingKeyFrames>\n");
    }
    styles += QStringLiteral("  </Storyboard>\n");
    return true;
}

} // namespace

bool ArtifactExportNoesisXamlWriter::write(const ArtifactCompositionPtr& composition,
                                           const QString& outputDirectory,
                                           const ArtifactNoesisXamlExportOptions& options,
                                           QString* errorMessage) {
    ArtifactExportSession session(composition);
    if (!session.build(errorMessage)) return false;
    QDir output(outputDirectory);
    if (!output.exists() && !QDir().mkpath(outputDirectory)) {
        if (errorMessage) *errorMessage = QStringLiteral("NoesisGUI出力先を作成できませんでした。");
        return false;
    }
    output.mkpath(QStringLiteral("assets"));
    if (!session.copyAssets(outputDirectory, errorMessage)) return false;
    const auto& source = session.snapshot();
    QHash<QString, QVector<int>> children;
    for (int i = 0; i < source.layers.size(); ++i) children[source.layers.at(i).parentId].push_back(i);
    const QString background = source.backgroundColor.name(QColor::HexArgb);
    QString xaml = QStringLiteral("<Canvas xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" Width=\"%1\" Height=\"%2\" Background=\"%3\" ClipToBounds=\"True\">\n<Canvas.Resources><ResourceDictionary Source=\"composition_styles.xaml\" /></Canvas.Resources>\n")
        .arg(source.size.width()).arg(source.size.height()).arg(background);
    QString styles = QStringLiteral("<ResourceDictionary xmlns=\"http://schemas.microsoft.com/winfx/2006/xaml/presentation\" xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">\n");
    bool appendOk = true;
    QString appendError;
    std::function<void(const QVector<int>&, int)> append = [&](const QVector<int>& indices, int depth) {
        for (const int index : indices) {
            const auto& layer = source.layers.at(index);
            if (!layer.visible) continue;
            const auto object = layer.serialized;
            const auto transform = object.value(QStringLiteral("transform")).toObject();
            const QString indent(depth * 2, QLatin1Char(' '));
            const QString id = safeId(layer.id);
            const double x = transform.value(QStringLiteral("px")).toDouble();
            const double y = transform.value(QStringLiteral("py")).toDouble();
            const double rotation = transform.value(QStringLiteral("rx")).toDouble();
            const double sx = transform.value(QStringLiteral("sx")).toDouble(1.0);
            const double sy = transform.value(QStringLiteral("sy")).toDouble(1.0);
            QString element;
            QStringList bakedAssetUrls;
            if (!ensureBakedSequence(session, layer, output, options.preRenderScale,
                                     &bakedAssetUrls, &appendError)) {
                appendOk = false;
                continue;
            }
            if (layer.requiresPreRender) {
                const QString bakedAssetUrl = bakedAssetUrls.isEmpty()
                    ? QString()
                    : bakedAssetUrls.front();
                const QSize bakedSize = QImageReader(output.filePath(bakedAssetUrl)).size();
                if (bakedSize.width() > 0 && bakedSize.height() > 0) {
                    element = QStringLiteral("<Image x:Name=\"%1\" Source=\"%2\" Width=\"%3\" Height=\"%4\" />")
                        .arg(id, bakedAssetUrl).arg(bakedSize.width()).arg(bakedSize.height());
                } else {
                    element = QStringLiteral("<Image x:Name=\"%1\" Source=\"%2\" />")
                        .arg(id, bakedAssetUrl);
                }
            } else if (object.contains(QStringLiteral("text.value"))) {
                const QColor textColor(object.value(QStringLiteral("text.color")).toString());
                const QString foreground = textColor.isValid()
                    ? textColor.name(QColor::HexArgb)
                    : QStringLiteral("#FFFFFFFF");
                const int alignment = object.value(QStringLiteral("text.alignment")).toInt(0);
                const QString textAlignment = alignment == 1 ? QStringLiteral("Center")
                    : alignment == 2 ? QStringLiteral("Right")
                    : alignment == 3 ? QStringLiteral("Justify")
                                     : QStringLiteral("Left");
                element = QStringLiteral("<TextBlock x:Name=\"%1\" Text=\"%2\" FontFamily=\"%3\" FontSize=\"%4\" Foreground=\"%5\" TextAlignment=\"%6\" />")
                    .arg(id, object.value(QStringLiteral("text.value")).toString().toHtmlEscaped(),
                         object.value(QStringLiteral("text.fontFamily")).toString(QStringLiteral("Arial")).toHtmlEscaped())
                    .arg(object.value(QStringLiteral("text.fontSize")).toDouble(24.0))
                    .arg(foreground)
                    .arg(textAlignment);
            } else if (object.contains(QStringLiteral("image.sourcePath"))) {
                const QString path = object.value(QStringLiteral("image.sourcePath")).toString();
                const QString assetUrl = session.assetPathFor(path);
                const int imageWidth = object.value(QStringLiteral("image.width")).toInt();
                const int imageHeight = object.value(QStringLiteral("image.height")).toInt();
                if (imageWidth > 0 && imageHeight > 0) {
                    element = QStringLiteral("<Image x:Name=\"%1\" Source=\"%2\" Width=\"%3\" Height=\"%4\" />")
                        .arg(id, assetUrl).arg(imageWidth).arg(imageHeight);
                } else {
                    element = QStringLiteral("<Image x:Name=\"%1\" Source=\"%2\" />")
                        .arg(id, assetUrl);
                }
            } else if (object.contains(QStringLiteral("shapeWidth"))) {
                const double width = object.value(QStringLiteral("shapeWidth")).toDouble();
                const double height = object.value(QStringLiteral("shapeHeight")).toDouble();
                const QString fill = object.value(QStringLiteral("fillEnabled")).toBool(true)
                    ? xmlColor(object, QStringLiteral("fill"))
                    : QStringLiteral("#00000000");
                if (object.value(QStringLiteral("shapeType")).toInt(0) == 1) {
                    element = QStringLiteral("<Ellipse x:Name=\"%1\" Width=\"%2\" Height=\"%3\" Fill=\"%4\" />")
                        .arg(id).arg(width).arg(height).arg(fill);
                } else {
                    element = QStringLiteral("<Rectangle x:Name=\"%1\" Width=\"%2\" Height=\"%3\" Fill=\"%4\" RadiusX=\"%5\" RadiusY=\"%5\" />")
                        .arg(id).arg(width).arg(height).arg(fill)
                        .arg(object.value(QStringLiteral("cornerRadius")).toDouble());
                }
            } else if (object.contains(QStringLiteral("solidColor"))) {
                const auto solid = object.value(QStringLiteral("solidColor")).toObject();
                element = QStringLiteral("<Rectangle x:Name=\"%1\" Width=\"%2\" Height=\"%3\" Fill=\"#%4%5%6%7\" />")
                    .arg(id).arg(object.value(QStringLiteral("solidWidth")).toDouble())
                    .arg(object.value(QStringLiteral("solidHeight")).toDouble())
                    .arg(qRound(solid.value(QStringLiteral("a")).toDouble(1.0) * 255.0), 2, 16, QLatin1Char('0'))
                    .arg(qRound(solid.value(QStringLiteral("r")).toDouble() * 255.0), 2, 16, QLatin1Char('0'))
                    .arg(qRound(solid.value(QStringLiteral("g")).toDouble() * 255.0), 2, 16, QLatin1Char('0'))
                    .arg(qRound(solid.value(QStringLiteral("b")).toDouble() * 255.0), 2, 16, QLatin1Char('0'));
            }
            if (!element.isEmpty()) {
                const QString targetName = id + QStringLiteral("_container");
                const bool hasStoryboard = layer.requiresPreRender
                    ? appendBakedStoryboard(styles, id, bakedAssetUrls,
                                            source.inPoint, source.outPoint,
                                            source.frameRate, layer.inPoint,
                                            layer.outPoint, layer.opacity)
                    : appendStoryboard(styles, layer, targetName,
                                       source.inPoint, source.outPoint,
                                       source.frameRate);
                const QString storyboardKey = layer.requiresPreRender
                    ? QStringLiteral("artifact_baked_%1").arg(id)
                    : QStringLiteral("artifact_anim_%1").arg(targetName);
                xaml += indent + QStringLiteral("<Canvas x:Name=\"%1\" Opacity=\"%2\" Canvas.Left=\"%3\" Canvas.Top=\"%4\" RenderTransformOrigin=\"0,0\"><Canvas.RenderTransform><TransformGroup><ScaleTransform x:Name=\"%1_scale\" ScaleX=\"%5\" ScaleY=\"%6\"/><RotateTransform x:Name=\"%1_rotate\" Angle=\"%7\"/></TransformGroup></Canvas.RenderTransform>")
                    .arg(targetName).arg(layer.opacity).arg(x).arg(y).arg(sx).arg(sy).arg(rotation);
                if (hasStoryboard) {
                    xaml += QStringLiteral("<Canvas.Triggers><EventTrigger RoutedEvent=\"FrameworkElement.Loaded\"><BeginStoryboard Storyboard=\"{StaticResource %1}\" /></EventTrigger></Canvas.Triggers>")
                        .arg(storyboardKey);
                }
                xaml += QStringLiteral("\n");
                xaml += QString((depth + 1) * 2, QLatin1Char(' ')) + element + QStringLiteral("\n");
            }
            append(children.value(layer.id), depth + 1);
            if (!element.isEmpty()) {
                xaml += indent + QStringLiteral("</Canvas>\n");
            }
        }
    };
    append(children.value(QString()), 1);
    if (!appendOk) {
        if (errorMessage) *errorMessage = appendError;
        return false;
    }
    xaml += QStringLiteral("</Canvas>\n");
    styles += QStringLiteral("</ResourceDictionary>\n");
    if (!writeText(output.filePath(QStringLiteral("composition.xaml")), xaml) ||
        !writeText(output.filePath(QStringLiteral("composition_styles.xaml")), styles)) {
        if (errorMessage) *errorMessage = QStringLiteral("NoesisGUIファイルを書き出せませんでした。");
        return false;
    }
    return true;
}

bool ArtifactExportNoesisXamlWriter::write(const ArtifactCompositionPtr& composition,
                                           const QString& outputDirectory,
                                           QString* errorMessage) {
    return ArtifactExportNoesisXamlWriter::write(
        composition, outputDirectory, ArtifactNoesisXamlExportOptions{}, errorMessage);
}

} // namespace Artifact
