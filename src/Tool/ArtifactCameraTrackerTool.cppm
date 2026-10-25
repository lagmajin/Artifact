module;

#include <QString>
#include <QVector>
#include <QImage>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <memory>
#include <type_traits>
#include <algorithm>
#include <cmath>
#include <utility>

export module Artifact.Tool.CameraTracker;

export import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Camera;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Tracking.CameraTracker;
import Frame.Position;
import Frame.Range;
import Time.Rational;
import Memory.SharedPtr;

namespace Artifact {

export class ArtifactCameraTrackerTool {
public:
    struct ProgressUpdate {
        int currentFrame;
        int totalFrames;
        QString message;
    };

    struct AnalysisResult {
        ArtifactCore::Tracking::CameraTrackResult tracking;
        std::int64_t startFrame = 0;
        std::int64_t endFrame = -1;

        bool isValid() const noexcept {
            return endFrame >= startFrame && tracking.success &&
                   tracking.cameraPath.size() >= 2;
        }
    };

    template<typename ProgressCallback>
    static AnalysisResult analyze(ArtifactAbstractComposition* comp,
                                  ArtifactAbstractLayerPtr videoLayer,
                                  ProgressCallback&& progress) {
        AnalysisResult analysis;
        if (!comp || !videoLayer) return analysis;

        const auto sourceLayer = ArtifactCore::dynamicPointerCast<ArtifactVideoLayer>(videoLayer);
        if (!sourceLayer) return analysis;

        const auto compositionRange = comp->frameRange();
        if (!compositionRange.isValid()) return analysis;
        const std::int64_t start = std::max(
            sourceLayer->inPoint(), compositionRange.start());
        const std::int64_t end = std::min(
            sourceLayer->outPoint(), compositionRange.end());
        if (end < start || end - start >= 100000) return analysis;
        const int total = static_cast<int>(end - start + 1);

        const QString sourcePath = sourceLayer->sourcePath();
        const QByteArray sourcePathBytes = sourcePath.toLocal8Bit();
        cv::VideoCapture sourceDecoder(sourcePathBytes.constData());
        if (sourcePath.isEmpty() || !sourceDecoder.isOpened()) return analysis;

        ArtifactCore::Tracking::CameraTracker tracker;
        int decodedFrameCount = 0;
        std::int64_t nextSourceFrame = -1;
        // Read sequential source frames with VideoCapture; CameraTracker sorts
        // by timeline timestamp when source time mapping is non-monotonic.
        for (int offset = 0; offset < total; ++offset) {
            const std::int64_t frame = start + offset;
            if constexpr (std::is_invocable_v<ProgressCallback,
                                              const ProgressUpdate&>) {
                progress({offset, total,
                          QStringLiteral("Analyzing frames…")});
            }

            const std::int64_t sourceFrame = sourceLayer->hasSourceTimeMapping()
                ? static_cast<std::int64_t>(std::llround(
                      sourceLayer->getSourceFrameAtCompFrame(frame)))
                : frame - sourceLayer->inPoint() +
                      sourceLayer->startTime().framePosition();
            if (sourceFrame < 0) continue;
            const bool needsSeek = sourceFrame != nextSourceFrame;
            const bool initialFrameAtStreamStart = nextSourceFrame < 0 &&
                                                   sourceFrame == 0;
            if (needsSeek && !initialFrameAtStreamStart &&
                !sourceDecoder.set(cv::CAP_PROP_POS_FRAMES,
                                   static_cast<double>(sourceFrame))) {
                nextSourceFrame = -1;
                continue;
            }
            cv::Mat decoded;
            const bool read = sourceDecoder.read(decoded);
            nextSourceFrame = read ? sourceFrame + 1 : -1;
            QImage image;
            if (read && !decoded.empty() && decoded.type() == CV_8UC3) {
                image = QImage(decoded.data, decoded.cols, decoded.rows,
                               static_cast<qsizetype>(decoded.step),
                               QImage::Format_BGR888).copy();
            } else if (read && !decoded.empty() && decoded.type() == CV_8UC4) {
                cv::Mat bgr;
                cv::cvtColor(decoded, bgr, cv::COLOR_BGRA2BGR);
                image = QImage(bgr.data, bgr.cols, bgr.rows,
                               static_cast<qsizetype>(bgr.step),
                               QImage::Format_BGR888).copy();
            } else if (read && !decoded.empty() && decoded.type() == CV_8UC1) {
                image = QImage(decoded.data, decoded.cols, decoded.rows,
                               static_cast<qsizetype>(decoded.step),
                               QImage::Format_Grayscale8).copy();
            }
            if (!image.isNull() && image.width() > 0 && image.height() > 0 &&
                image.width() <= 16384 && image.height() <= 16384) {
                tracker.addFrame(static_cast<double>(frame), image);
                ++decodedFrameCount;
            }
        }
        sourceDecoder.release();
        if (total <= 0 || decodedFrameCount < 2) return analysis;

        if constexpr (std::is_invocable_v<ProgressCallback,
                                          const ProgressUpdate&>) {
            progress({total, total, QStringLiteral("Solving camera pose…")});
        }
        analysis.tracking = tracker.solve();
        analysis.startFrame = start;
        analysis.endFrame = end;
        return analysis;
    }

    static bool apply(ArtifactAbstractComposition* comp,
                      const AnalysisResult& analysis) {
        if (!comp || !analysis.isValid()) return false;
        const auto compositionRange = comp->frameRange();
        const double fps = comp->frameRate().framerate();
        if (!compositionRange.isValid() || !std::isfinite(fps) ||
            fps <= 0.0 || fps > 240.0) {
            return false;
        }
        const bool hasFinitePose = std::any_of(
            analysis.tracking.cameraPath.begin(),
            analysis.tracking.cameraPath.end(), [](const auto& pose) {
                return std::isfinite(pose.time) &&
                       std::isfinite(static_cast<double>(pose.position.x())) &&
                       std::isfinite(static_cast<double>(pose.position.y())) &&
                       std::isfinite(static_cast<double>(pose.position.z())) &&
                       std::isfinite(static_cast<double>(pose.rotation.x())) &&
                       std::isfinite(static_cast<double>(pose.rotation.y())) &&
                       std::isfinite(static_cast<double>(pose.rotation.z()));
            });
        if (!hasFinitePose) return false;

        ArtifactLayerFactory factory;
        ArtifactLayerInitParams cameraParams(QStringLiteral("Tracked Camera"),
                                             LayerType::Camera);
        auto cameraLayer = ArtifactCore::dynamicPointerCast<ArtifactCameraLayer>(
            factory.createNewLayer(cameraParams));
        if (!cameraLayer) return false;

        int keyedPoseCount = 0;
        auto& cameraTransform = cameraLayer->transform3D();
        for (const auto& pose : analysis.tracking.cameraPath) {
            if (!std::isfinite(pose.time) ||
                !std::isfinite(static_cast<double>(pose.position.x())) ||
                !std::isfinite(static_cast<double>(pose.position.y())) ||
                !std::isfinite(static_cast<double>(pose.position.z())) ||
                !std::isfinite(static_cast<double>(pose.rotation.x())) ||
                !std::isfinite(static_cast<double>(pose.rotation.y())) ||
                !std::isfinite(static_cast<double>(pose.rotation.z()))) {
                continue;
            }
            const auto frame = static_cast<std::int64_t>(std::llround(pose.time));
            if (frame < compositionRange.start() ||
                frame > compositionRange.end()) {
                continue;
            }
            const auto time = cameraLayer->keyframeTimeAtFrame(frame);
            cameraTransform.setPosition(time, pose.position.x(), pose.position.y());
            cameraTransform.setPositionZ(time, pose.position.z());
            cameraTransform.setRotationX(time, pose.rotation.x());
            cameraTransform.setRotationY(time, pose.rotation.y());
            cameraTransform.setRotationZ(time, pose.rotation.z());
            ++keyedPoseCount;
        }
        if (keyedPoseCount < 2) return false;

        cameraLayer->setActiveCamera(true);
        if (!comp->appendLayerTop(cameraLayer).success) return false;

        int createdFeatureLayers = 0;
        for (const auto& point : analysis.tracking.featurePoints) {
            if (createdFeatureLayers >= 1024) break;
            if (!point.isValid) continue;
            ArtifactLayerInitParams nullParams(
                QStringLiteral("Track Point %1").arg(point.id), LayerType::Null);
            auto nullLayer = factory.createNewLayer(nullParams);
            if (!nullLayer) continue;
            nullLayer->transform3D().setCurrentPosition(
                point.position.x(), point.position.y());
            nullLayer->transform3D().setCurrentPositionZ(point.position.z());
            if (comp->appendLayerTop(nullLayer).success) ++createdFeatureLayers;
        }
        return true;
    }

    static bool run(ArtifactAbstractComposition* comp,
                    ArtifactAbstractLayerPtr videoLayer) {
        return run(comp, std::move(videoLayer),
                   [](const ProgressUpdate&) {});
    }

    template<typename ProgressCallback>
    static bool run(ArtifactAbstractComposition* comp,
                    ArtifactAbstractLayerPtr videoLayer,
                    ProgressCallback&& progress) {
        auto analysis = analyze(comp, std::move(videoLayer),
                                std::forward<ProgressCallback>(progress));
        return apply(comp, analysis);
    }
};

} // namespace Artifact
