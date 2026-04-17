module;

#include <QString>
#include <QVector>
#include <QImage>
#include <memory>

export module Artifact.Tool.CameraTracker;

import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Factory;
import Tracking.CameraTracker;
import Frame.Position;
import Frame.Range;

namespace Artifact {

export class ArtifactCameraTrackerTool {
public:
    struct ProgressUpdate {
        int currentFrame;
        int totalFrames;
        QString message;
    };

    using ProgressCallback = std::function<void(const ProgressUpdate&)>;

    static bool run(ArtifactAbstractComposition* comp, 
                    ArtifactAbstractLayerPtr videoLayer,
                    ProgressCallback progress = nullptr) {
        if (!comp || !videoLayer) return false;

        const auto sourceLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(videoLayer);
        if (!sourceLayer) {
            return false;
        }

        ArtifactCore::Tracking::CameraTracker tracker;
        
        // 解析範囲の設定
        const int64_t start = sourceLayer->inPoint();
        const int64_t end = sourceLayer->outPoint();
        int total = static_cast<int>(end - start + 1);

        // 1. 各フレームの画像を収集してトラッカーに送る
        for (int64_t f = start; f <= end; ++f) {
            if (progress) {
                progress({static_cast<int>(f - start), total, "Analyzing frames..."});
            }
            
            // 動画レイヤーからフレーム画像を取得
            QImage img = sourceLayer->decodeFrameToQImage(f);
            if (!img.isNull()) {
                tracker.addFrame(static_cast<double>(f), img);
            }
        }

        // 2. 解析実行
        if (progress) {
            progress({total, total, "Solving camera pose..."});
        }
        auto result = tracker.solve();

        if (!result.success) return false;

        // 3. 結果をコンポジションに反映
        ArtifactLayerFactory factory;

        // カメラレイヤーの作成
        ArtifactLayerInitParams camParams(QString("Tracked Camera"), LayerType::Camera);
        auto cameraLayer = factory.createNewLayer(camParams);
        if (cameraLayer) {
            comp->appendLayerTop(cameraLayer);
        }

        // 3D特徴点の作成 (Nullレイヤー)
        for (const auto& pt : result.featurePoints) {
            if (!pt.isValid) continue;
            
            ArtifactLayerInitParams nullParams(QString("Track Point %1").arg(pt.id), LayerType::Null);
            auto nullLayer = factory.createNewLayer(nullParams);
            if (nullLayer) {
                comp->appendLayerTop(nullLayer);
            }
        }

        return true;
    }
};

} // namespace Artifact
