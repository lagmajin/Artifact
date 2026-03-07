module;
#include <QVector>
#include <QMatrix4x4>
#include <QColor>
#include <memory>
#include <QVector3D>

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
export module Artifact.Effect.Clone.Core;




import Artifact.Effect.Abstract;
import Utils.String.UniString;

export namespace Artifact {

    // ─────────────────────────────────────────────────────────
    // CloneData
    // クローン（複製物）1つ1つの状態を保持する構造体
    // 単なる行列ではなく、エフェクターによって操作されるすべての属性を持つ
    // ─────────────────────────────────────────────────────────
    struct CloneData {
        int index = 0;                  // クローンの通し番号
        QMatrix4x4 transform;           // 位置・回転・スケール
        QColor color = Qt::white;       // ブレンドカラー
        float weight = 1.0f;            // エフェクターの影響度 (0.0 - 1.0)
        float timeOffset = 0.0f;        // 再生時間のオフセット（秒）
        bool visible = true;            // 表示・非表示
    };

    // ─────────────────────────────────────────────────────────
    // AbstractCloneField (フォールオフ)
    // 空間座標を受け取り、0.0 〜 1.0 のウェイト(影響度)を返す基底クラス
    // (例: 距離ベースの球状フィールド、線形フィールドなど)
    // ─────────────────────────────────────────────────────────
    class AbstractCloneField {
    public:
        virtual ~AbstractCloneField() = default;
        virtual float sample(const QVector3D& position) const = 0;
    };

    // ─────────────────────────────────────────────────────────
    // AbstractCloneEffector
    // クローン配列全体を受け取り、非破壊で操作(変形・色変え)を行う基底クラス
    // ─────────────────────────────────────────────────────────
    class AbstractCloneEffector : public ArtifactAbstractEffect {
    public:
        AbstractCloneEffector() {
            setPipelineStage(EffectPipelineStage::PreProcess); // Transform計算後、描画前
        }
        virtual ~AbstractCloneEffector() = default;

        // エフェクターの影響範囲を決めるフィールドを追加
        void addField(std::shared_ptr<AbstractCloneField> field) {
            fields_.push_back(field);
        }

        // 評価: 前のジェネレーター/エフェクターから来たクローン配列を操作して返す
        virtual void applyToClones(std::vector<CloneData>& clones) const = 0;

    protected:
        // フィールドを合成して、指定位置での最終的な影響ウェイトを計算する
        float calculateFieldWeight(const QVector3D& position) const {
            if (fields_.empty()) return 1.0f; // フィールドがない場合は全体に100%影響
            
            float totalWeight = 0.0f;
            // 簡易的に加算ブレンド (本来はMax, Min, Add等ブレンドモードがあるべき)
            for (const auto& f : fields_) {
                totalWeight += f->sample(position);
            }
            return std::clamp(totalWeight, 0.0f, 1.0f);
        }

    private:
        std::vector<std::shared_ptr<AbstractCloneField>> fields_;
    };

}
