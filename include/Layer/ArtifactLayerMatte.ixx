module;
#include <algorithm>
#include <cmath>
#include <utility>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

export module Artifact.Layer.Matte;

import Utils.Id;
import Utils.String.UniString;
import Layer.Matte;

export namespace Artifact {

    // マットの抽出元をどう扱うか (AEのトラックマットに相当)
    enum class MatteType {
        Alpha,
        Luma,
        InverseAlpha,
        InverseLuma
    };

    // 複数のマットをどう合成するか (AEのマスクモードに相当)
    enum class MatteBlendMode {
        Add,
        Subtract,
        Intersect,
        Difference
    };

    // アセットのサイズがレイヤーと異なる場合のフィット方法
    enum class MatteFitMode {
        Stretch,  // レイヤーのサイズに引き伸ばす
        Fit,      // アスペクト比を維持して収める
        Fill,     // アスペクト比を維持して全体を覆う
        Original  // アセットの元サイズのまま中央配置
    };

    struct LayerMatteReference {
        ArtifactCore::Id id;               // マット自体のユニークID (UI操作用)
        ArtifactCore::Id sourceLayerId;    // 参照するレイヤーのID (AEのトラックマット)
        QString sourceAssetPath;           // Project Input Source の明示的な互換パス
        bool enabled = true;
        MatteType type = MatteType::Alpha;
        MatteBlendMode blendMode = MatteBlendMode::Add;
        MatteFitMode fitMode = MatteFitMode::Stretch;
        float opacity = 1.0f;              // マットの適用強度 (0.0 - 1.0)
        bool invert = false;               // 最終的な反転フラグ

        LayerMatteReference() {
            id = ArtifactCore::Id(); // Auto-generate ID
        }

        QJsonObject toJson() const {
            QJsonObject obj;
            obj["id"] = id.toString();
            obj["sourceLayerId"] = sourceLayerId.toString();
            if (!sourceAssetPath.isEmpty()) obj["sourceAssetPath"] = sourceAssetPath;
            obj["enabled"] = enabled;
            obj["type"] = static_cast<int>(type);
            obj["blendMode"] = static_cast<int>(blendMode);
            obj["fitMode"] = static_cast<int>(fitMode);
            obj["opacity"] = static_cast<double>(opacity);
            obj["invert"] = invert;
            return obj;
        }

        void fromJson(const QJsonObject& obj) {
            const QString idValue = obj["id"].toString().trimmed();
            if (!idValue.isEmpty()) {
                ArtifactCore::Id parsedId(idValue);
                if (!parsedId.isNil()) {
                    id = parsedId;
                }
            }
            const QString sourceLayerValue =
                obj["sourceLayerId"].toString().trimmed();
            if (!sourceLayerValue.isEmpty()) {
                sourceLayerId = ArtifactCore::Id(sourceLayerValue);
            } else if (obj.contains("assetId")) {
                // legacy: migrate old assetId to sourceLayerId
                sourceLayerId = ArtifactCore::Id(obj["assetId"].toString());
            } else {
                sourceLayerId = ArtifactCore::Id();
            }
            sourceAssetPath = obj["sourceAssetPath"].toString().trimmed();
            enabled = obj["enabled"].toBool(true);
            const auto validEnumValue = [](const QJsonValue& value,
                                           int count, int fallback) {
                const int parsed = value.toInt(fallback);
                return parsed >= 0 && parsed < count ? parsed : fallback;
            };
            type = static_cast<MatteType>(validEnumValue(
                obj["type"], 4, static_cast<int>(MatteType::Alpha)));
            blendMode = static_cast<MatteBlendMode>(validEnumValue(
                obj["blendMode"], 4, static_cast<int>(MatteBlendMode::Add)));
            fitMode = static_cast<MatteFitMode>(validEnumValue(
                obj["fitMode"], 4, static_cast<int>(MatteFitMode::Stretch)));
            const double parsedOpacity = obj["opacity"].toDouble(1.0);
            opacity = std::isfinite(parsedOpacity)
                ? static_cast<float>(std::clamp(parsedOpacity, 0.0, 1.0))
                : 1.0f;
            invert = obj["invert"].toBool(false);
        }

        // Core MatteMode への変換
        ArtifactCore::MatteMode toCoreMatteMode() const {
            if (invert) {
                switch (type) {
                case MatteType::Alpha: return ArtifactCore::MatteMode::AlphaInverted;
                case MatteType::Luma: return ArtifactCore::MatteMode::LuminanceInverted;
                case MatteType::InverseAlpha: return ArtifactCore::MatteMode::Alpha;
                case MatteType::InverseLuma: return ArtifactCore::MatteMode::Luminance;
                }
            }
            switch (type) {
            case MatteType::Alpha: return ArtifactCore::MatteMode::Alpha;
            case MatteType::Luma: return ArtifactCore::MatteMode::Luminance;
            case MatteType::InverseAlpha: return ArtifactCore::MatteMode::AlphaInverted;
            case MatteType::InverseLuma: return ArtifactCore::MatteMode::LuminanceInverted;
            }
            return ArtifactCore::MatteMode::Alpha;
        }

        // Core MatteNode への変換
        ArtifactCore::MatteNode toCoreMatteNode() const {
            ArtifactCore::MatteNode node;
            node.setSourceLayerId(sourceLayerId);
            node.setMode(toCoreMatteMode());
            node.setEnabled(enabled);
            return node;
        }
    };
}
