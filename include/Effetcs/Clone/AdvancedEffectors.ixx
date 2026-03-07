module;
#include <QVector3D>
#include <QMatrix4x4>
#include <QColor>
#include <cmath>
#include <random>

export module Artifact.Effect.Clone.Advanced;

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
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>



import Artifact.Effect.Clone.Core;

export namespace Artifact {

    // ─────────────────────────────────────────────────────────
    // LinearCloneField (リニアフォールオフ)
    // 指定した方向ベクトルに沿って、0.0から1.0へとウェイトが変化する
    // ─────────────────────────────────────────────────────────
    class LinearCloneField : public AbstractCloneField {
    public:
        QVector3D startPoint{-100, 0, 0};
        QVector3D endPoint{100, 0, 0};
        bool clampWeight = true; // 0.0と1.0でクランプするかどうか

        float sample(const QVector3D& position) const override {
            QVector3D dir = endPoint - startPoint;
            float lengthSq = dir.lengthSquared();
            
            if (lengthSq < 0.0001f) return 1.0f; // 始点と終点が同じ場合

            // startPointを原点とした時のpositionの、dir方向への投影（内積）
            QVector3D p = position - startPoint;
            float t = QVector3D::dotProduct(p, dir) / lengthSq;

            if (clampWeight) {
                t = std::clamp(t, 0.0f, 1.0f);
            }
            return t;
        }
    };

    // ─────────────────────────────────────────────────────────
    // StepCloneEffector (ステップエフェクター)
    // クローンのインデックス（順番）に比例して、段階的にオフセットを適用する
    // 例：100個のクローンに対して、1個目から100個目に向かってスケールが大きくなる
    // ─────────────────────────────────────────────────────────
    class StepCloneEffector : public AbstractCloneEffector {
    public:
        QVector3D positionStep{0, 0, 0};
        QVector3D rotationStep{0, 0, 0};
        QVector3D scaleStep{0, 0, 0};
        float timeOffsetStep = 0.0f; // 順番に出現させるための時間差

        // Splineカーブを使って変化の度合いをコントロールすることも可能（今回は線形）

        void applyToClones(std::vector<CloneData>& clones) const override {
            if (clones.empty()) return;
            
            int count = clones.size();
            for (int i = 0; i < count; ++i) {
                auto& clone = clones[i];
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                
                // インデックスに基づく進行度 (0.0 〜 1.0)
                float progress = static_cast<float>(i) / std::max(1, count - 1);
                
                // 最終的な影響度 (フィールド × 進行度)
                float finalWeight = clone.weight * fieldWeight * progress;

                if (finalWeight <= 0.0001f) continue;

                if (!positionStep.isNull()) {
                    clone.transform.translate(positionStep * finalWeight);
                }
                if (!rotationStep.isNull()) {
                    clone.transform.rotate(rotationStep.x() * finalWeight, 1, 0, 0);
                    clone.transform.rotate(rotationStep.y() * finalWeight, 0, 1, 0);
                    clone.transform.rotate(rotationStep.z() * finalWeight, 0, 0, 1);
                }
                if (!scaleStep.isNull()) {
                    float sx = 1.0f + (scaleStep.x() * finalWeight);
                    float sy = 1.0f + (scaleStep.y() * finalWeight);
                    float sz = 1.0f + (scaleStep.z() * finalWeight);
                    clone.transform.scale(sx, sy, sz);
                }
                
                // アニメーションの時間差オフセットを追加
                clone.timeOffset += timeOffsetStep * finalWeight;
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // RandomCloneEffector (ランダムエフェクター)
    // クローンのインデックスをシードにして、個別にランダムなオフセットを与える
    // ─────────────────────────────────────────────────────────
    class RandomCloneEffector : public AbstractCloneEffector {
    public:
        QVector3D positionVariance{0, 0, 0}; // 例: (50, 50, 50) なら -50〜50の範囲でブレる
        QVector3D rotationVariance{0, 0, 0};
        float scaleVariance = 0.0f; // 例: 0.5 なら 0.5倍〜1.5倍の範囲でブレる
        int seed = 12345;

        // 簡単なハッシュベースの擬似乱数ジェネレータ
        float hash(int n) const {
            n = (n << 13) ^ n;
            return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
        }

        void applyToClones(std::vector<CloneData>& clones) const override {
            for (auto& clone : clones) {
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                float finalWeight = clone.weight * fieldWeight;

                if (finalWeight <= 0.0001f) continue;

                // 各クローンの固有シードを生成
                int cloneSeed = seed + clone.index * 1337;

                // ランダムな位置オフセット (-1.0 ~ 1.0)
                float rx = hash(cloneSeed) * positionVariance.x() * finalWeight;
                float ry = hash(cloneSeed + 1) * positionVariance.y() * finalWeight;
                float rz = hash(cloneSeed + 2) * positionVariance.z() * finalWeight;
                clone.transform.translate(rx, ry, rz);

                // ランダムな回転
                if (!rotationVariance.isNull()) {
                    float rotX = hash(cloneSeed + 3) * rotationVariance.x() * finalWeight;
                    float rotY = hash(cloneSeed + 4) * rotationVariance.y() * finalWeight;
                    float rotZ = hash(cloneSeed + 5) * rotationVariance.z() * finalWeight;
                    clone.transform.rotate(rotX, 1, 0, 0);
                    clone.transform.rotate(rotY, 0, 1, 0);
                    clone.transform.rotate(rotZ, 0, 0, 1);
                }

                // ランダムなスケール
                if (scaleVariance > 0.0f) {
                    float s = 1.0f + (hash(cloneSeed + 6) * scaleVariance * finalWeight);
                    clone.transform.scale(s, s, s);
                }
            }
        }
    };

}
