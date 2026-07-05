module;
#include <memory>

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
#include <QVector>
#include <QMatrix4x4>
#include <QColor>
#include <QVector3D>
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
        int sourceIndex = 0;            // 素材コンテナー参照用の素材インデックス
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
    // EffectorBlendMode
    // エフェクター同士の合成方法
    // ─────────────────────────────────────────────────────────
    export enum class EffectorBlendMode {
        Add = 0,
        Subtract,
        Multiply,
        Max,
        Min,
        Average,
        Normal
    };

    // ─────────────────────────────────────────────────────────
    // ブレンドユーティリティ関数
    // ─────────────────────────────────────────────────────────
    export inline void blendCloneData(
        CloneData& base,
        const CloneData& effector,
        EffectorBlendMode mode,
        float strength)
    {
        if (strength <= 0.001f) return;
        float s = std::clamp(strength, 0.0f, 1.0f);

        QVector3D basePos = base.transform.column(3).toVector3D();
        QVector3D effPos = effector.transform.column(3).toVector3D();

        switch (mode) {
        case EffectorBlendMode::Add:
            basePos += (effPos - basePos) * s;
            break;
        case EffectorBlendMode::Subtract:
            basePos -= (effPos - basePos) * s;
            break;
        case EffectorBlendMode::Multiply:
            {
                const float baseLength = basePos.length() > 0.001f ? basePos.length() : 1.0f;
                const QVector3D normalizedEffPos = effPos / baseLength;
                basePos.setX(basePos.x() * (1.0f + (normalizedEffPos.x() - 1.0f) * s));
                basePos.setY(basePos.y() * (1.0f + (normalizedEffPos.y() - 1.0f) * s));
                basePos.setZ(basePos.z() * (1.0f + (normalizedEffPos.z() - 1.0f) * s));
            }
            break;
        case EffectorBlendMode::Max:
            basePos.setX(std::max(basePos.x(), effPos.x() * s));
            basePos.setY(std::max(basePos.y(), effPos.y() * s));
            basePos.setZ(std::max(basePos.z(), effPos.z() * s));
            break;
        case EffectorBlendMode::Min:
            basePos.setX(std::min(basePos.x(), effPos.x() * (s + 0.001f)));
            basePos.setY(std::min(basePos.y(), effPos.y() * (s + 0.001f)));
            basePos.setZ(std::min(basePos.z(), effPos.z() * (s + 0.001f)));
            break;
        case EffectorBlendMode::Average:
            basePos = (basePos + effPos * s) * 0.5f;
            break;
        case EffectorBlendMode::Normal:
            basePos = basePos * (1.0f - s) + effPos * s;
            break;
        }
        base.transform.setColumn(3, QVector4D(basePos, 1.0f));

        if (effector.color != Qt::white) {
            float r, g, b, a;
            base.color.getRgbF(&r, &g, &b, &a);
            float er, eg, eb, ea;
            effector.color.getRgbF(&er, &eg, &eb, &ea);
            base.color.setRgbF(
                std::clamp(r + (er - r) * s, 0.0f, 1.0f),
                std::clamp(g + (eg - g) * s, 0.0f, 1.0f),
                std::clamp(b + (eb - b) * s, 0.0f, 1.0f),
                std::clamp(a + (ea - a) * s, 0.0f, 1.0f));
        }

        base.weight = base.weight * (1.0f - s) + effector.weight * s;
        if (!effector.visible) base.visible = false;
    }

    // ─────────────────────────────────────────────────────────
    // AbstractCloneEffector
    // クローン配列全体を受け取り、操作(変形・色変え)を行う基底クラス
    // ─────────────────────────────────────────────────────────
    class AbstractCloneEffector : public ArtifactAbstractEffect {
    public:
        AbstractCloneEffector() {
            setPipelineStage(EffectPipelineStage::PreProcess);
        }
        virtual ~AbstractCloneEffector() = default;

        EffectorBlendMode blendMode = EffectorBlendMode::Add;
        float strength = 1.0f;

        void addField(std::shared_ptr<AbstractCloneField> field) {
            fields_.push_back(field);
        }

        virtual void applyToClones(std::vector<CloneData>& clones) const = 0;

    protected:
        float calculateFieldWeight(const QVector3D& position) const {
            if (fields_.empty()) return 1.0f;
            float totalWeight = 0.0f;
            for (const auto& f : fields_) {
                totalWeight += f->sample(position);
            }
            return std::clamp(totalWeight, 0.0f, 1.0f);
        }

    private:
        std::vector<std::shared_ptr<AbstractCloneField>> fields_;
    };

}
