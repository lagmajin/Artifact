module;
#include <utility>
#include <QJsonObject>

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
            obj["enabled"] = enabled;
            obj["type"] = static_cast<int>(type);
            obj["blendMode"] = static_cast<int>(blendMode);
            obj["fitMode"] = static_cast<int>(fitMode);
            obj["opacity"] = static_cast<double>(opacity);
            obj["invert"] = invert;
            return obj;
        }

        void fromJson(const QJsonObject& obj) {
            if (obj.contains("id")) {
                id = ArtifactCore::Id(obj["id"].toString());
            }
            if (obj.contains("sourceLayerId")) {
                sourceLayerId = ArtifactCore::Id(obj["sourceLayerId"].toString());
            } else if (obj.contains("assetId")) {
                // legacy: migrate old assetId to sourceLayerId
                sourceLayerId = ArtifactCore::Id(obj["assetId"].toString());
            }
            enabled = obj["enabled"].toBool(true);
            type = static_cast<MatteType>(obj["type"].toInt(static_cast<int>(MatteType::Alpha)));
            blendMode = static_cast<MatteBlendMode>(obj["blendMode"].toInt(static_cast<int>(MatteBlendMode::Add)));
            fitMode = static_cast<MatteFitMode>(obj["fitMode"].toInt(static_cast<int>(MatteFitMode::Stretch)));
            opacity = static_cast<float>(obj["opacity"].toDouble(1.0));
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
