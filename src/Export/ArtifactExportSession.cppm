module;
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSize>
#include <QString>
#include <algorithm>
#include <utility>

module Artifact.Export.Session;

import Artifact.Layer.Abstract;
import Artifact.Render.CompositionViewDrawing;

namespace Artifact {

namespace {

bool needsExportRasterization(const QJsonObject& serialized) {
    const int type = serialized.value(QStringLiteral("type")).toInt(0);
    if (type == 8 || type == 10 || type == 15 || type == 16 ||
        type == 17 || type == 18 || type == 22 || type >= 23) {
        return true;
    }
    if (serialized.contains(QStringLiteral("svg.sourcePath")) ||
        !serialized.value(QStringLiteral("image.sequencePaths")).toArray().isEmpty()) {
        return true;
    }
    if (type == 4 && serialized.value(QStringLiteral("image.sourcePath")).toString().isEmpty()) {
        return true;
    }
    const auto layerType = serialized.value(QStringLiteral("layerType")).toString();
    if (layerType == QStringLiteral("Shape")) {
        const int shapeType = serialized.value(QStringLiteral("shapeType")).toInt(0);
        return shapeType >= 2 ||
               !serialized.value(QStringLiteral("customPath")).toArray().isEmpty() ||
               !serialized.value(QStringLiteral("shapeOperators")).toArray().isEmpty() ||
               serialized.value(QStringLiteral("fillType")).toInt(0) != 0 ||
               serialized.value(QStringLiteral("strokeEnabled")).toBool(false) ||
               serialized.value(QStringLiteral("strokeGradientEnabled")).toBool(false);
    }
    if (serialized.contains(QStringLiteral("solidColor"))) {
        return serialized.value(QStringLiteral("solidFillType")).toInt(0) != 0;
    }
    return false;
}

} // namespace

ArtifactExportSession::ArtifactExportSession(ArtifactCompositionPtr composition)
    : composition_(std::move(composition)) {}

bool ArtifactExportSession::build(QString* errorMessage) {
    snapshot_ = {};
    built_ = false;
    if (!composition_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("コンポジションが指定されていません。");
        }
        return false;
    }

    const QSize size = composition_->effectiveCompositionSize();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("コンポジションサイズが不正です。");
        }
        return false;
    }

    const auto range = composition_->frameRange();
    snapshot_.name = composition_->settings().compositionName().toQString();
    if (snapshot_.name.trimmed().isEmpty()) {
        snapshot_.name = QStringLiteral("composition_%1").arg(composition_->id().toString());
    }
    snapshot_.size = size;
    const auto background = composition_->backgroundColor();
    snapshot_.backgroundColor = QColor::fromRgbF(
        std::clamp(static_cast<double>(background.r()), 0.0, 1.0),
        std::clamp(static_cast<double>(background.g()), 0.0, 1.0),
        std::clamp(static_cast<double>(background.b()), 0.0, 1.0),
        std::clamp(static_cast<double>(background.a()), 0.0, 1.0));
    snapshot_.frameRate = std::max<double>(
        1.0, static_cast<double>(composition_->frameRate().framerate()));
    snapshot_.inPoint = std::max(0, static_cast<int>(range.start()));
    snapshot_.outPoint = std::max(snapshot_.inPoint + 1,
                                  static_cast<int>(range.end()));

    const auto layers = composition_->allLayer();
    QHash<QString, QString> sourceToRelativePath;
    QHash<QString, int> usedAssetNames;
    const auto sanitizeAssetPart = [](QString value, const QString& fallback) {
        value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
        return value.isEmpty() ? fallback : value;
    };
    const auto collectAsset = [&](const QString& sourcePath) {
        if (sourcePath.trimmed().isEmpty()) return;
        const QFileInfo info(sourcePath);
        const QString sourceKey = info.absoluteFilePath();
        if (sourceToRelativePath.contains(sourceKey)) return;

        const bool hasSuffix = !info.suffix().isEmpty();
        const QString suffix = hasSuffix
            ? sanitizeAssetPart(info.suffix(), QStringLiteral("asset"))
            : QString();
        const QString stem = sanitizeAssetPart(info.completeBaseName(), QStringLiteral("asset"));
        QString fileName = stem;
        if (hasSuffix) {
            fileName += QStringLiteral(".") + suffix;
        }
        int ordinal = usedAssetNames.value(fileName, 0);
        while (usedAssetNames.contains(fileName)) {
            ++ordinal;
            fileName = QStringLiteral("%1_%2%3")
                .arg(stem).arg(ordinal)
                .arg(suffix.isEmpty() ? QString() : QStringLiteral(".%1").arg(suffix));
        }
        usedAssetNames.insert(fileName, ordinal);
        sourceToRelativePath.insert(sourceKey, QStringLiteral("assets/%1").arg(fileName));

        ArtifactExportAssetSnapshot asset;
        asset.sourcePath = sourcePath;
        asset.relativePath = sourceToRelativePath.value(sourceKey);
        snapshot_.assets.push_back(std::move(asset));
    };

    snapshot_.layers.reserve(layers.size());
    for (const auto& layer : layers) {
        if (!layer) {
            continue;
        }
        ArtifactExportLayerSnapshot exported;
        exported.serialized = layer->toJson();
        exported.id = layer->id().toString();
        exported.name = layer->layerName();
        // A newly-created layer can carry an uninitialized LayerID until it
        // is explicitly parented. Use the resolved relationship so root
        // layers remain reachable by every writer's tree walk.
        if (const auto parent = layer->parentLayer()) {
            exported.parentId = parent->id().toString();
        }
        exported.type = exported.serialized.value(QStringLiteral("type")).toInt();
        exported.visible = layer->isVisible();
        exported.is3D = layer->is3D();
        const auto matteReferences = layer->matteReferences();
        const auto layerId = layer->id();
        const bool hasActiveMatte = std::any_of(
            matteReferences.cbegin(), matteReferences.cend(),
            [layerId](const LayerMatteReference& ref) {
                return ref.enabled && !ref.sourceLayerId.isNil() &&
                       ref.sourceLayerId != layerId;
            });
        exported.requiresPreRender = exported.is3D ||
                                     layerHasCpuRasterizerWork(layer.get()) ||
                                     layer->hasMasks() ||
                                     hasActiveMatte ||
                                     !layer->getEffects().empty() ||
                                     needsExportRasterization(exported.serialized);
        if (!exported.requiresPreRender) {
            collectAsset(exported.serialized.value(QStringLiteral("image.sourcePath")).toString());
        }
        if (exported.is3D) {
            exported.preRenderReason = QStringLiteral("3Dレイヤー");
        } else if (layer->hasMasks()) {
            exported.preRenderReason = QStringLiteral("マスク");
        } else if (hasActiveMatte) {
            exported.preRenderReason = QStringLiteral("マット");
        } else if (layerHasCpuRasterizerWork(layer.get())) {
            exported.preRenderReason = QStringLiteral("ラスターエフェクト");
        } else if (!layer->getEffects().empty()) {
            exported.preRenderReason = QStringLiteral("エフェクト");
        } else if (needsExportRasterization(exported.serialized)) {
            const auto serialized = exported.serialized;
            if (serialized.contains(QStringLiteral("video.sourcePath"))) {
                exported.preRenderReason = QStringLiteral("動画レイヤー");
            } else if (serialized.contains(QStringLiteral("svg.sourcePath"))) {
                exported.preRenderReason = QStringLiteral("SVGレイヤー");
            } else if (!serialized.value(QStringLiteral("image.sequencePaths")).toArray().isEmpty()) {
                exported.preRenderReason = QStringLiteral("画像シーケンス");
            } else if (serialized.value(QStringLiteral("type")).toInt(0) == 4) {
                exported.preRenderReason = QStringLiteral("埋め込み画像");
            } else {
                exported.preRenderReason = QStringLiteral("複雑シェイプ、グラデーションまたは線");
            }
        }
        exported.blendMode = static_cast<int>(layer->layerBlendType());
        exported.opacity = std::clamp(static_cast<double>(layer->opacity()), 0.0, 1.0);
        const int rawInPoint = static_cast<int>(layer->inPoint().framePosition());
        const int rawOutPoint = static_cast<int>(layer->outPoint().framePosition());
        exported.inPoint = std::clamp(rawInPoint, snapshot_.inPoint,
                                      snapshot_.outPoint - 1);
        exported.outPoint = std::clamp(rawOutPoint, exported.inPoint + 1,
                                       snapshot_.outPoint);
        const auto transform = exported.serialized.value(QStringLiteral("transform")).toObject();
        const bool hasTransformKeyframes =
            !transform.value(QStringLiteral("positionKeyframes")).toArray().isEmpty() ||
            !transform.value(QStringLiteral("rotationKeyframes")).toArray().isEmpty() ||
            !transform.value(QStringLiteral("scaleKeyframes")).toArray().isEmpty();
        snapshot_.hasTransformKeyframes = snapshot_.hasTransformKeyframes || hasTransformKeyframes;
        const bool is3D = exported.is3D;
        const QString layerName = exported.name;
        const bool hasEffects = !exported.serialized.value(QStringLiteral("effects")).toArray().isEmpty();
        const QString imageSourcePath = exported.serialized.value(QStringLiteral("image.sourcePath")).toString();
        const QString svgSourcePath = exported.serialized.value(QStringLiteral("svg.sourcePath")).toString();
        const QString videoSourcePath = exported.serialized.value(QStringLiteral("video.sourcePath")).toString();
        snapshot_.layers.push_back(std::move(exported));

        if (is3D) {
            snapshot_.warnings.push_back(
                QStringLiteral("3Dレイヤー '%1' はフレーム列としてプリレンダーされます。")
                    .arg(layerName));
        }
        if (snapshot_.layers.back().requiresPreRender) {
            snapshot_.warnings.push_back(
                QStringLiteral("レイヤー '%1' は%2のためプリレンダー対象です。")
                    .arg(layerName, snapshot_.layers.back().preRenderReason));
        }
        if (hasEffects) {
            snapshot_.warnings.push_back(
                QStringLiteral("レイヤー '%1' のエフェクトはネイティブ変換せずプリレンダーされます。").arg(layerName));
        }
        const QString sourcePath = !imageSourcePath.isEmpty()
            ? imageSourcePath
            : !svgSourcePath.isEmpty() ? svgSourcePath : videoSourcePath;
        if (!sourcePath.isEmpty() && !QFileInfo::exists(sourcePath)) {
            snapshot_.warnings.push_back(
                QStringLiteral("レイヤー '%1' のソースが見つかりません: %2")
                    .arg(layerName, sourcePath));
        }
    }

    built_ = true;
    return true;
}

bool ArtifactExportSession::isBuilt() const noexcept {
    return built_;
}

const ArtifactExportSnapshot& ArtifactExportSession::snapshot() const noexcept {
    return snapshot_;
}

ArtifactCompositionPtr ArtifactExportSession::composition() const noexcept {
    return composition_;
}

QString ArtifactExportSession::assetPathFor(const QString& sourcePath) const {
    if (sourcePath.trimmed().isEmpty()) return {};
    const QFileInfo info(sourcePath);
    const QString sourceKey = info.absoluteFilePath();
    for (const auto& asset : snapshot_.assets) {
        if (QFileInfo(asset.sourcePath).absoluteFilePath() == sourceKey) {
            return asset.relativePath;
        }
    }
    return QStringLiteral("assets/%1").arg(info.fileName());
}

bool ArtifactExportSession::copyAssets(const QString& outputDirectory,
                                       QString* errorMessage) const {
    if (outputDirectory.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("アセット出力先が指定されていません。");
        return false;
    }
    QDir output(outputDirectory);
    if (!output.mkpath(QStringLiteral("assets"))) {
        if (errorMessage) *errorMessage = QStringLiteral("アセット出力先を作成できませんでした。");
        return false;
    }
    for (const auto& asset : snapshot_.assets) {
        if (!QFileInfo::exists(asset.sourcePath)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("画像アセットが見つかりません: %1")
                    .arg(asset.sourcePath);
            }
            return false;
        }
        const QString target = output.filePath(asset.relativePath);
        if (QFileInfo::exists(target)) continue;
        if (!QFile::copy(asset.sourcePath, target)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("画像アセットをコピーできませんでした: %1")
                    .arg(asset.sourcePath);
            }
            return false;
        }
    }
    return true;
}

} // namespace Artifact
