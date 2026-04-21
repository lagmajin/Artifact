module;
#include <cmath>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
export module Artifact.Effect.Clone.Basic;




import Artifact.Effect.Clone.Core;

export namespace Artifact {

    // ─────────────────────────────────────────────────────────
    // SphericalCloneField (球状フォールオフ)
    // 指定した位置・半径の球内に入ったクローンにウェイトを与える
    // ─────────────────────────────────────────────────────────
    class SphericalCloneField : public AbstractCloneField {
    public:
        QVector3D center{0, 0, 0};
        float radius = 100.0f;
        float falloff = 0.5f; // 0.0=ハードエッジ, 1.0=中心から徐々に減衰

        float sample(const QVector3D& position) const override {
            if (radius <= 0.0f) return 0.0f;

            float distance = position.distanceToPoint(center);
            if (distance >= radius) return 0.0f;

            float innerRadius = radius * (1.0f - falloff);
            if (distance <= innerRadius) return 1.0f;

            // スムーズな減衰 (Smoothstep相当)
            float t = (distance - innerRadius) / (radius - innerRadius);
            t = std::clamp(1.0f - t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    };

    // ─────────────────────────────────────────────────────────
    // TransformCloneEffector (プレーンエフェクター相当)
    // フィールドのウェイトに応じて、TransformとColorを相対的に変化させる
    // ─────────────────────────────────────────────────────────
    class TransformCloneEffector : public AbstractCloneEffector {
    public:
        // オフセットパラメータ
        QVector3D positionOffset{0, 0, 0};
        QVector3D rotationOffset{0, 0, 0}; // Euler angles (pitch, yaw, roll)
        QVector3D scaleOffset{0, 0, 0};    // 0 = 変化なし, 1.0 = 2倍
        
        QColor colorOffset = Qt::transparent; // Alpha > 0 の場合ブレンドされる
        bool useColor = false;

        TransformCloneEffector() {
            setDisplayName(ArtifactCore::UniString("Transform Effector"));
        }

        void applyToClones(std::vector<CloneData>& clones) const override {
            for (auto& clone : clones) {
                // クローンの現在のワールド位置を抽出 (簡易的)
                QVector3D currentPos = clone.transform.column(3).toVector3D();

                // フィールドからの総合ウェイトを計算
                float totalWeight = calculateFieldWeight(currentPos);
                
                // 本エフェクター自身の強度(0.0-1.0)と掛け合わせる
                float finalWeight = clone.weight * totalWeight;

                if (finalWeight <= 0.001f) continue; // 影響なしならスキップ

                // 位置のオフセット
                if (!positionOffset.isNull()) {
                    clone.transform.translate(positionOffset * finalWeight);
                }

                // 回転のオフセット
                if (!rotationOffset.isNull()) {
                    clone.transform.rotate(rotationOffset.x() * finalWeight, 1, 0, 0);
                    clone.transform.rotate(rotationOffset.y() * finalWeight, 0, 1, 0);
                    clone.transform.rotate(rotationOffset.z() * finalWeight, 0, 0, 1);
                }

                // スケールのオフセット (1.0 + (scale * weight))
                if (!scaleOffset.isNull()) {
                    float sx = 1.0f + (scaleOffset.x() * finalWeight);
                    float sy = 1.0f + (scaleOffset.y() * finalWeight);
                    float sz = 1.0f + (scaleOffset.z() * finalWeight);
                    clone.transform.scale(sx, sy, sz);
                }

                // カラーのブレンド
                if (useColor && colorOffset.alpha() > 0) {
                    float blend = (colorOffset.alphaF() * finalWeight);
                    float r = clone.color.redF() * (1.0f - blend) + colorOffset.redF() * blend;
                    float g = clone.color.greenF() * (1.0f - blend) + colorOffset.greenF() * blend;
                    float b = clone.color.blueF() * (1.0f - blend) + colorOffset.blueF() * blend;
                    clone.color.setRgbF(r, g, b, clone.color.alphaF());
                }
            }
        }
    };

}
