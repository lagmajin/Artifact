module;

#include <QString>
#include <QPointF>
#include <vector>
#include <cmath>
#include <utility>

export module Artifact.Tool.PointTracker;

export import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Tracking.MotionTracker;
import Animation.Transform3D;
import Time.Rational;

namespace Artifact {

export class ArtifactPointTrackerTool {
public:
    struct ApplyOptions {
        int pointId = 0;
        bool createNullLayer = true;       ///< Null レイヤーを新規作成する
        bool writeAnchor = true;           ///< アンカーポイントを追跡点中心に設定する
        bool applyToSelectedLayer = false;  ///< 選択中レイヤーに直接書き出す
    };

    /// トラッキング結果をコンポジションに適用する。
    ///
    /// MotionTracker の NCC 結果を Null レイヤーの位置キーフレームとして書き出す。
    /// createNullLayer=true の場合は新規 Null レイヤーを作成し、
    /// applyToSelectedLayer=true の場合は targetLayer に直接書き出す。
    static bool applyTrackingResult(
        ArtifactAbstractComposition* comp,
        const ArtifactCore::MotionTracker& tracker,
        const ApplyOptions& options,
        ArtifactAbstractLayerPtr targetLayer = nullptr)
    {
        if (!comp) return false;

        const auto keyframes = tracker.exportKeyframes(options.pointId);
        if (keyframes.empty()) return false;

        const float fps = comp->frameRate().framerate();
        if (fps <= 0.0f) return false;

        ArtifactAbstractLayerPtr writeLayer;

        if (options.applyToSelectedLayer && targetLayer) {
            writeLayer = targetLayer;
        } else if (options.createNullLayer) {
            ArtifactLayerFactory factory;
            ArtifactLayerInitParams nullParams(
                QStringLiteral("Track Point %1").arg(options.pointId),
                LayerType::Null);
            writeLayer = factory.createNewLayer(nullParams);
            if (!writeLayer) return false;
            comp->appendLayerTop(writeLayer);
        } else {
            return false;
        }

        auto& t3d = writeLayer->transform3D();

        bool isFirst = true;
        for (const auto& [timeSeconds, pos] : keyframes) {
            // time (double seconds) → frame number → RationalTime
            const int64_t frame = static_cast<int64_t>(std::round(timeSeconds * fps));
            const ArtifactCore::RationalTime rt(frame, static_cast<int64_t>(fps));

            t3d.setPosition(rt, static_cast<float>(pos.x()), static_cast<float>(pos.y()));

            // 初期フレームでアンカーポイントを追跡点中心に設定
            if (options.writeAnchor && isFirst) {
                t3d.setAnchor(rt, static_cast<float>(pos.x()), static_cast<float>(pos.y()));
                isFirst = false;
            }
        }

        return true;
    }

    /// 全トラッキングポイントの結果をそれぞれ個別の Null レイヤーに書き出す。
    static int applyAllTrackingPoints(
        ArtifactAbstractComposition* comp,
        const ArtifactCore::MotionTracker& tracker,
        const ApplyOptions& options)
    {
        if (!comp) return 0;

        const auto result = tracker.result();
        if (result.frames.empty()) return 0;

        // 全ポイント ID を収集
        std::vector<int> pointIds;
        for (const auto& frame : result.frames) {
            for (const auto& pt : frame.points) {
                if (pt.active) {
                    if (pointIds.empty() || pointIds.back() != pt.id) {
                        pointIds.push_back(pt.id);
                    }
                }
            }
        }

        int applied = 0;
        for (int id : pointIds) {
            ApplyOptions opts = options;
            opts.pointId = id;
            if (applyTrackingResult(comp, tracker, opts)) {
                ++applied;
            }
        }

        return applied;
    }
};

} // namespace Artifact
