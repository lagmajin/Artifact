module;
#include <cmath>
#include <random>

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
export module Artifact.Effect.Clone.Advanced;




import Artifact.Effect.Clone.Core;
import Math.Interpolate;

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
        float timeOffsetStep = 0.0f;
        ArtifactCore::InterpolationType easeType = ArtifactCore::InterpolationType::Linear;

        void applyToClones(std::vector<CloneData>& clones) const override {
            if (clones.empty()) return;
            
            int count = clones.size();
            for (int i = 0; i < count; ++i) {
                auto& clone = clones[i];
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                
                // インデックスに基づく進行度 (0.0 〜 1.0)
                float progress = static_cast<float>(i) / std::max(1, count - 1);
                progress = ArtifactCore::interpolate(0.0f, 1.0f, progress, easeType);
                
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

    // ─────────────────────────────────────────────────────────
    // DelayCloneEffector (遅延エフェクター)
    // クローン配列上を波のように変化が伝播するエフェクトを生成する
    // モード: Forward / Reverse / ForwardReverse / Pendulum
    // ─────────────────────────────────────────────────────────
    class DelayCloneEffector : public AbstractCloneEffector {
    public:
        enum class DelayMode {
            None = 0,
            Forward,
            Reverse,
            ForwardReverse,
            Pendulum
        };

        DelayMode delayMode = DelayMode::Forward;
        float delayRate = 0.1f;        // 伝播速度 (0.0-1.0)
        float delayStrength = 1.0f;    // 影響の強さ (0.0-1.0)
        float currentTime = 0.0f;      // アニメーション時間（外から毎フレーム更新）

        QVector3D positionDelay{0,0,0};
        QVector3D rotationDelay{0,0,0};
        QVector3D scaleDelay{0,0,0};

        void applyToClones(std::vector<CloneData>& clones) const override {
            if (clones.empty() || delayMode == DelayMode::None) return;

            int count = static_cast<int>(clones.size());
            float duration = 1.0f / std::max(delayRate, 0.001f);

            for (int i = 0; i < count; ++i) {
                auto& clone = clones[i];
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                float finalWeight = clone.weight * fieldWeight * delayStrength;
                if (finalWeight <= 0.0001f) continue;

                // インデックスに基づく時間オフセット (0.0 - 1.0)
                float idxT = static_cast<float>(i) / std::max(1, count - 1);

                // モードごとの遅延時間計算
                float localTime = 0.0f;
                switch (delayMode) {
                case DelayMode::Forward:
                    localTime = idxT;
                    break;
                case DelayMode::Reverse:
                    localTime = 1.0f - idxT;
                    break;
                case DelayMode::ForwardReverse:
                    localTime = (idxT < 0.5f) ? idxT * 2.0f : (1.0f - idxT) * 2.0f;
                    break;
                case DelayMode::Pendulum:
                    localTime = std::sin(idxT * static_cast<float>(M_PI) * 2.0f) * 0.5f + 0.5f;
                    break;
                default:
                    localTime = 0.0f;
                    break;
                }

                // 現在時間とローカル時間の差が伝播ディレイ
                float wave = std::clamp((currentTime - localTime * duration) / duration, 0.0f, 1.0f);
                float ease = wave * wave * (3.0f - 2.0f * wave); // smoothstep

                if (!positionDelay.isNull()) {
                    clone.transform.translate(positionDelay * ease * finalWeight);
                }
                if (!rotationDelay.isNull()) {
                    clone.transform.rotate(rotationDelay.x() * ease * finalWeight, 1, 0, 0);
                    clone.transform.rotate(rotationDelay.y() * ease * finalWeight, 0, 1, 0);
                    clone.transform.rotate(rotationDelay.z() * ease * finalWeight, 0, 0, 1);
                }
                if (!scaleDelay.isNull()) {
                    float sx = 1.0f + (scaleDelay.x() * ease * finalWeight);
                    float sy = 1.0f + (scaleDelay.y() * ease * finalWeight);
                    float sz = 1.0f + (scaleDelay.z() * ease * finalWeight);
                    clone.transform.scale(sx, sy, sz);
                }
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // SoundCloneEffector (サウンドエフェクター)
    // オーディオ振幅/スペクトルをクローンの変形にマッピングする
    // 外部から audioAmplitude / audioLowBand / audioMidBand / audioHighBand を
    // 毎フレーム更新して使用する
    // ─────────────────────────────────────────────────────────
    class SoundCloneEffector : public AbstractCloneEffector {
    public:
        enum class FrequencyMode {
            Full = 0,
            Low,
            Mid,
            High
        };

        // Audio input (外部から更新)
        float audioAmplitude = 0.0f;    // マスター振幅 (0.0-1.0)
        float audioLowBand = 0.0f;      // 低域 (0.0-1.0)
        float audioMidBand = 0.0f;      // 中域 (0.0-1.0)
        float audioHighBand = 0.0f;     // 高域 (0.0-1.0)

        // Envelope follower (アタック/リリース)
        float attackMs = 10.0f;         // アタック時間 (ms)
        float releaseMs = 100.0f;       // リリース時間 (ms)
        float deltaTime = 0.0f;         // 毎フレーム設定する経過時間

        // Frequency selection
        FrequencyMode frequencyMode = FrequencyMode::Full;

        // Mapping amounts
        QVector3D positionAmount{0, 0, 0};     // 振幅→位置マッピング量
        QVector3D rotationAmount{0, 0, 0};     // 振幅→回転マッピング量
        QVector3D scaleAmount{0, 0, 0};        // 振幅→スケールマッピング量
        QColor colorAmount{Qt::white};         // 振幅→色マッピング量
        bool useWeightMap = false;             // 振幅→weight
        bool useVisibilityMap = false;         // 振幅→可視/不可視
        float visibilityThreshold = 0.1f;      // この振幅未満で不可視

        // Clone delay (波状伝播)
        float cloneDelay = 0.0f;               // インデックス遅延 (0=なし, 大=遅い)

        // Energy flooring (silence gate)
        float floorThreshold = 0.0f;           // この値未満の振幅を0にクランプ

    private:
        mutable float smoothedAmplitude_ = 0.0f;
        mutable float smoothedLow_ = 0.0f;
        mutable float smoothedMid_ = 0.0f;
        mutable float smoothedHigh_ = 0.0f;

        float getEffectiveAmplitude() const {
            switch (frequencyMode) {
            case FrequencyMode::Low:  return smoothedLow_;
            case FrequencyMode::Mid:  return smoothedMid_;
            case FrequencyMode::High: return smoothedHigh_;
            default:                  return smoothedAmplitude_;
            }
        }

        void smooth(float& smoothed, float raw, float ms) const {
            if (ms <= 0.0f || deltaTime <= 0.0f) { smoothed = raw; return; }
            float tau = ms * 0.001f;
            float alpha = std::clamp(deltaTime / tau, 0.0f, 1.0f);
            smoothed += (raw - smoothed) * alpha;
        }

    public:
        void applyToClones(std::vector<CloneData>& clones) const override {
            // Envelope follower
            smooth(smoothedAmplitude_, audioAmplitude,
                   audioAmplitude > smoothedAmplitude_ ? attackMs : releaseMs);
            smooth(smoothedLow_, audioLowBand,
                   audioLowBand > smoothedLow_ ? attackMs : releaseMs);
            smooth(smoothedMid_, audioMidBand,
                   audioMidBand > smoothedMid_ ? attackMs : releaseMs);
            smooth(smoothedHigh_, audioHighBand,
                   audioHighBand > smoothedHigh_ ? attackMs : releaseMs);

            float amp = getEffectiveAmplitude();
            if (amp < floorThreshold) amp = 0.0f;

            int count = static_cast<int>(clones.size());
            for (int i = 0; i < count; ++i) {
                auto& clone = clones[i];
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                float finalWeight = clone.weight * fieldWeight;
                if (finalWeight <= 0.0001f) continue;

                // Clone delay (wave propagation along indices)
                float delayedAmp = amp;
                if (cloneDelay > 0.0f && count > 1) {
                    float idxT = static_cast<float>(i) / static_cast<float>(count - 1);
                    float delayOffset = std::clamp(1.0f - idxT * cloneDelay, 0.0f, 1.0f);
                    // Simple delayed amplitude estimate
                    float envelope = smoothedAmplitude_ * delayOffset;
                    float raw = audioAmplitude * delayOffset;
                    delayedAmp = std::lerp(raw, envelope, 0.5f);
                    if (delayedAmp < floorThreshold) delayedAmp = 0.0f;
                }

                // Position
                if (!positionAmount.isNull()) {
                    clone.transform.translate(
                        positionAmount.x() * delayedAmp * finalWeight,
                        positionAmount.y() * delayedAmp * finalWeight,
                        positionAmount.z() * delayedAmp * finalWeight);
                }

                // Rotation
                if (!rotationAmount.isNull()) {
                    clone.transform.rotate(rotationAmount.x() * delayedAmp * finalWeight, 1, 0, 0);
                    clone.transform.rotate(rotationAmount.y() * delayedAmp * finalWeight, 0, 1, 0);
                    clone.transform.rotate(rotationAmount.z() * delayedAmp * finalWeight, 0, 0, 1);
                }

                // Scale
                if (!scaleAmount.isNull()) {
                    float sx = 1.0f + (scaleAmount.x() * delayedAmp * finalWeight);
                    float sy = 1.0f + (scaleAmount.y() * delayedAmp * finalWeight);
                    float sz = 1.0f + (scaleAmount.z() * delayedAmp * finalWeight);
                    clone.transform.scale(sx, sy, sz);
                }

                // Color
                if (colorAmount != Qt::white) {
                    float r = std::clamp(clone.color.redF()   + colorAmount.redF()   * delayedAmp * finalWeight, 0.0f, 1.0f);
                    float g = std::clamp(clone.color.greenF() + colorAmount.greenF() * delayedAmp * finalWeight, 0.0f, 1.0f);
                    float b = std::clamp(clone.color.blueF()  + colorAmount.blueF()  * delayedAmp * finalWeight, 0.0f, 1.0f);
                    clone.color.setRgbF(r, g, b);
                }

                // Weight
                if (useWeightMap) {
                    clone.weight *= delayedAmp;
                }

                // Visibility
                if (useVisibilityMap) {
                    clone.visible = (delayedAmp >= visibilityThreshold);
                }
            }
        }
    };

    // ─────────────────────────────────────────────────────────
    // NoiseCloneEffector (ノイズエフェクター)
    // パーリンノイズ等を用いて、時間やインデックスに応じて滑らかなランダム変化を与える
    // ─────────────────────────────────────────────────────────
    class NoiseCloneEffector : public AbstractCloneEffector {
    public:
        QVector3D positionAmplitude{50, 50, 0};
        QVector3D rotationAmplitude{0, 0, 45};
        float scaleAmplitude = 0.0f;
        float frequency = 1.0f;
        float timeSpeed = 1.0f;
        float currentTime = 0.0f;
        int seed = 54321;

        // 簡易的なノイズ関数 (Sin合成による擬似ノイズ)
        float noise(float x, float y, float z) const {
            auto s = [](float v) { return std::sin(v); };
            return (s(x) + s(y + 1.23f) + s(z + 2.34f)) / 3.0f;
        }

        void applyToClones(std::vector<CloneData>& clones) const override {
            for (auto& clone : clones) {
                QVector3D currentPos = clone.transform.column(3).toVector3D();
                float fieldWeight = calculateFieldWeight(currentPos);
                float finalWeight = clone.weight * fieldWeight;

                if (finalWeight <= 0.0001f) continue;

                // ノイズのサンプリング座標 (インデックス + 時間)
                float sampleX = static_cast<float>(clone.index) * frequency + seed;
                float sampleY = currentTime * timeSpeed;
                float sampleZ = seed * 0.1f;

                float nx = noise(sampleX, sampleY, sampleZ);
                float ny = noise(sampleX + 10.0f, sampleY, sampleZ);
                float nz = noise(sampleX + 20.0f, sampleY, sampleZ);

                if (!positionAmplitude.isNull()) {
                    clone.transform.translate(nx * positionAmplitude.x() * finalWeight,
                                            ny * positionAmplitude.y() * finalWeight,
                                            nz * positionAmplitude.z() * finalWeight);
                }

                if (!rotationAmplitude.isNull()) {
                    clone.transform.rotate(nx * rotationAmplitude.x() * finalWeight, 1, 0, 0);
                    clone.transform.rotate(ny * rotationAmplitude.y() * finalWeight, 0, 1, 0);
                    clone.transform.rotate(nz * rotationAmplitude.z() * finalWeight, 0, 0, 1);
                }

                if (scaleAmplitude != 0.0f) {
                    float s = 1.0f + (nx * scaleAmplitude * finalWeight);
                    clone.transform.scale(s, s, s);
                }
            }
        }
    };

}

