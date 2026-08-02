module;

#include <QString>
#include <QVector>
#include <QImage>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <cmath>

export module Artifact.Tool.CameraTracker;

export import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Factory;
import Tracking.CameraTracker;
import Frame.Position;
import Frame.Range;
import Memory.SharedPtr;

namespace Artifact {

export class ArtifactCameraTrackerTool {
public:
    struct ProgressUpdate {
        int currentFrame;
        int totalFrames;
        QString message;
    };

    static bool run(ArtifactAbstractComposition* comp,
                    ArtifactAbstractLayerPtr videoLayer) {
        return run(comp, videoLayer, [](const ProgressUpdate&) {});
    }

    template<typename ProgressCallback>
    static bool run(ArtifactAbstractComposition* comp,
                    ArtifactAbstractLayerPtr videoLayer,
                    ProgressCallback&& progress) {
        if (!comp || !videoLayer) return false;

        const auto sourceLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(videoLayer);
        if (!sourceLayer) {
            return false;
        }

        ArtifactCore::Tracking::CameraTracker tracker;
        
        // 解析範囲の設定
        const auto compositionRange = comp->frameRange();
        if (!compositionRange.isValid()) return false;
        const int64_t start = std::max(sourceLayer->inPoint(), compositionRange.start());
        const int64_t end = std::min(sourceLayer->outPoint(), compositionRange.end());
        if (end < start || end - start >= 100000) return false;
        const int total = static_cast<int>(end - start + 1);

        // 1. 各フレームの画像を収集してトラッカーに送る
        int decodedFrameCount = 0;
        for (int64_t f = start; f <= end; ++f) {
            if constexpr (std::is_invocable_v<ProgressCallback, const ProgressUpdate&>) {
                progress({static_cast<int>(f - start), total, "Analyzing frames..."});
            }
            
            // 動画レイヤーからフレーム画像を取得
            QImage img = sourceLayer->decodeFrameToQImage(f);
            if (!img.isNull() && img.width() > 0 && img.height() > 0 &&
                img.width() <= 16384 && img.height() <= 16384) {
                tracker.addFrame(static_cast<double>(f), img);
                ++decodedFrameCount;
            }
        }

        if (total <= 0 || decodedFrameCount < 2) return false;

        // 2. 解析実行
        if constexpr (std::is_invocable_v<ProgressCallback, const ProgressUpdate&>) {
            progress({total, total, "Solving camera pose..."});
        }
        auto result = tracker.solve();

        if (!result.success || result.cameraPath.empty()) return false;
        const bool hasFinitePose = std::any_of(
            result.cameraPath.begin(), result.cameraPath.end(), [](const auto& pose) {
                return std::isfinite(pose.time) &&
                       std::isfinite(static_cast<double>(pose.position.x())) &&
                       std::isfinite(static_cast<double>(pose.position.y())) &&
                       std::isfinite(static_cast<double>(pose.position.z())) &&
                       std::isfinite(static_cast<double>(pose.rotation.x())) &&
                       std::isfinite(static_cast<double>(pose.rotation.y())) &&
                       std::isfinite(static_cast<double>(pose.rotation.z()));
            });
        if (!hasFinitePose) return false;

        // 3. 結果をコンポジションに反映
        ArtifactLayerFactory factory;

        // カメラレイヤーの作成
        ArtifactLayerInitParams camParams(QString("Tracked Camera"), LayerType::Camera);
        auto cameraLayer = factory.createNewLayer(camParams);
        if (cameraLayer) {
            comp->appendLayerTop(cameraLayer);
        }

        // 3D特徴点の作成 (Nullレイヤー)
        int createdFeatureLayers = 0;
        for (const auto& pt : result.featurePoints) {
            if (createdFeatureLayers >= 1024) break;
            if (!pt.isValid) continue;
            
            ArtifactLayerInitParams nullParams(QString("Track Point %1").arg(pt.id), LayerType::Null);
            auto nullLayer = factory.createNewLayer(nullParams);
            if (nullLayer) {
                comp->appendLayerTop(nullLayer);
                ++createdFeatureLayers;
            }
        }

        return true;
    }
};

} // namespace Artifact
