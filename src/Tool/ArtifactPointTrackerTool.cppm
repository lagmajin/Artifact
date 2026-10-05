module;

#include <QString>
#include <QPointF>
#include <vector>
#include <set>
#include <array>
#include <algorithm>
#include <cmath>
#include <utility>
#include <QVariant>
#include <memory>

export module Artifact.Tool.PointTracker;

export import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Tracking.MotionTracker;
import Animation.Transform3D;
import Time.Rational;
import Artifact.Service.Effect;
import Artifact.Effect.Abstract;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Undo.UndoManager;

namespace Artifact {

namespace {

/// Solves the 3x3 homography H with H * p ~= q for four point correspondences.
/// Uses the standard 8x8 formulation with H(2,2) fixed to 1 and Gaussian
/// elimination with partial pivoting.  Returns false when the system is
/// singular (degenerate or collinear quad) or a coefficient is not finite.
bool solveHomography4Point(const std::array<QPointF, 4>& source,
                           const std::array<QPointF, 4>& target,
                           double matrix[9])
{
    double rows[8][9] = {};
    for (int i = 0; i < 4; ++i) {
        const double x = source[static_cast<size_t>(i)].x();
        const double y = source[static_cast<size_t>(i)].y();
        const double u = target[static_cast<size_t>(i)].x();
        const double v = target[static_cast<size_t>(i)].y();
        if (!std::isfinite(x) || !std::isfinite(y) ||
            !std::isfinite(u) || !std::isfinite(v)) {
            return false;
        }
        rows[2 * i + 0][0] = x;  rows[2 * i + 0][1] = y;  rows[2 * i + 0][2] = 1.0;
        rows[2 * i + 0][6] = -u * x; rows[2 * i + 0][7] = -u * y; rows[2 * i + 0][8] = u;
        rows[2 * i + 1][3] = x;  rows[2 * i + 1][4] = y;  rows[2 * i + 1][5] = 1.0;
        rows[2 * i + 1][6] = -v * x; rows[2 * i + 1][7] = -v * y; rows[2 * i + 1][8] = v;
    }
    for (int col = 0; col < 8; ++col) {
        int pivot = col;
        for (int row = col + 1; row < 8; ++row) {
            if (std::abs(rows[row][col]) > std::abs(rows[pivot][col])) {
                pivot = row;
            }
        }
        if (std::abs(rows[pivot][col]) < 1.0e-12) {
            return false;
        }
        if (pivot != col) {
            for (int k = 0; k < 9; ++k) {
                std::swap(rows[col][k], rows[pivot][k]);
            }
        }
        for (int row = col + 1; row < 8; ++row) {
            const double factor = rows[row][col] / rows[col][col];
            for (int k = col; k < 9; ++k) {
                rows[row][k] -= factor * rows[col][k];
            }
        }
    }
    double solution[8] = {};
    for (int row = 7; row >= 0; --row) {
        double sum = rows[row][8];
        for (int k = row + 1; k < 8; ++k) {
            sum -= rows[row][k] * solution[k];
        }
        solution[row] = sum / rows[row][row];
        if (!std::isfinite(solution[row])) {
            return false;
        }
    }
    matrix[0] = solution[0]; matrix[1] = solution[1]; matrix[2] = solution[2];
    matrix[3] = solution[3]; matrix[4] = solution[4]; matrix[5] = solution[5];
    matrix[6] = solution[6]; matrix[7] = solution[7]; matrix[8] = 1.0;
    return true;
}

/// Applies a row-major 3x3 homography.  Returns a null QPointF when the point
/// maps behind the horizon (w ~ 0) or to a non-finite coordinate, so callers can
/// reject the whole frame instead of writing a broken vertex.
QPointF applyHomography3x3(const double matrix[9], const QPointF& point)
{
    const double x = point.x();
    const double y = point.y();
    const double w = matrix[6] * x + matrix[7] * y + matrix[8];
    if (std::abs(w) < 1.0e-12) {
        return QPointF();
    }
    const double px = (matrix[0] * x + matrix[1] * y + matrix[2]) / w;
    const double py = (matrix[3] * x + matrix[4] * y + matrix[5]) / w;
    if (!std::isfinite(px) || !std::isfinite(py)) {
        return QPointF();
    }
    return QPointF(px, py);
}

} // namespace

export class ArtifactPointTrackerTool {
public:
    struct ApplyOptions {
        int pointId = -1;              ///< -1 は最初に登録された点を使用
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
        const ArtifactCompositionPtr& comp,
        const ArtifactCore::MotionTracker& tracker,
        const ApplyOptions& options,
        ArtifactAbstractLayerPtr targetLayer = nullptr)
    {
        if (!comp) return false;

        const int pointId = options.pointId >= 0
                                ? options.pointId
                                : tracker.firstTrackPointId();
        if (pointId < 0) return false;

        const auto exportedKeyframes = tracker.exportKeyframes(pointId);
        std::vector<std::pair<double, QPointF>> keyframes;
        keyframes.reserve(exportedKeyframes.size());
        for (const auto& [timeSeconds, pos] : exportedKeyframes) {
            if (std::isfinite(timeSeconds) && std::abs(timeSeconds) <= 1.0e9 &&
                std::isfinite(pos.x()) && std::isfinite(pos.y()) &&
                std::abs(pos.x()) <= 1.0e9 && std::abs(pos.y()) <= 1.0e9) {
                keyframes.push_back({timeSeconds, pos});
            }
        }
        if (keyframes.empty()) return false;

        const float fps = comp->frameRate().framerate();
        if (!std::isfinite(fps) || fps <= 0.0f || fps > 240.0f) return false;
        const auto compositionRange = comp->frameRange();
        if (!compositionRange.isValid()) return false;

        ArtifactAbstractLayerPtr writeLayer;
        bool createdNullLayer = false;

        if (options.applyToSelectedLayer && targetLayer) {
            writeLayer = targetLayer;
        } else if (options.createNullLayer) {
            ArtifactLayerFactory factory;
            ArtifactLayerInitParams nullParams(
                QStringLiteral("Track Point %1").arg(pointId),
                LayerType::Null);
            writeLayer = factory.createNewLayer(nullParams);
            if (!writeLayer) return false;
            createdNullLayer = true;
        } else {
            return false;
        }

        auto& t3d = writeLayer->transform3D();

        const auto positionXProperty =
            writeLayer->getProperty(QStringLiteral("transform.position.x"));
        const auto positionYProperty =
            writeLayer->getProperty(QStringLiteral("transform.position.y"));
        const auto anchorXProperty =
            writeLayer->getProperty(QStringLiteral("transform.anchor.x"));
        const auto anchorYProperty =
            writeLayer->getProperty(QStringLiteral("transform.anchor.y"));
        if (!positionXProperty || !positionYProperty ||
            (options.writeAnchor && (!anchorXProperty || !anchorYProperty))) {
            return false;
        }
        const auto beforePositionX = positionXProperty->getKeyFrames();
        const auto beforePositionY = positionYProperty->getKeyFrames();
        const auto beforeAnchorX = anchorXProperty
            ? anchorXProperty->getKeyFrames()
            : std::vector<ArtifactCore::KeyFrame>{};
        const auto beforeAnchorY = anchorYProperty
            ? anchorYProperty->getKeyFrames()
            : std::vector<ArtifactCore::KeyFrame>{};

        bool isFirst = true;
        bool appliedAny = false;
        for (const auto& [timeSeconds, pos] : keyframes) {
            // time (double seconds) → frame number → RationalTime
            const int64_t frame = static_cast<int64_t>(std::round(timeSeconds * fps));
            if (frame < compositionRange.start() || frame > compositionRange.end()) {
                continue;
            }
            const ArtifactCore::RationalTime rt(frame, static_cast<int64_t>(fps));

            t3d.setPosition(rt, static_cast<float>(pos.x()), static_cast<float>(pos.y()));
            appliedAny = true;

            // 初期フレームでアンカーポイントを追跡点中心に設定
            if (options.writeAnchor && isFirst) {
                t3d.setAnchor(rt, static_cast<float>(pos.x()), static_cast<float>(pos.y()));
                isFirst = false;
            }
        }

        if (!appliedAny) return false;

        const auto afterPositionX = positionXProperty->getKeyFrames();
        const auto afterPositionY = positionYProperty->getKeyFrames();
        const auto afterAnchorX = anchorXProperty
            ? anchorXProperty->getKeyFrames()
            : std::vector<ArtifactCore::KeyFrame>{};
        const auto afterAnchorY = anchorYProperty
            ? anchorYProperty->getKeyFrames()
            : std::vector<ArtifactCore::KeyFrame>{};

        if (auto* undo = UndoManager::instance()) {
            const auto sameKeyframes = [](const auto& lhs, const auto& rhs) {
                if (lhs.size() != rhs.size()) return false;
                for (std::size_t index = 0; index < lhs.size(); ++index) {
                    const auto& a = lhs[index];
                    const auto& b = rhs[index];
                    if (a.time != b.time || a.value != b.value ||
                        a.interpolation != b.interpolation ||
                        a.cp1_x != b.cp1_x || a.cp1_y != b.cp1_y ||
                        a.cp2_x != b.cp2_x || a.cp2_y != b.cp2_y ||
                        a.roving != b.roving || a.anchor != b.anchor ||
                        a.colorLabel != b.colorLabel) {
                        return false;
                    }
                }
                return true;
            };
            auto restoreProperty = [](const ArtifactAbstractLayerPtr& layer,
                                      const QString& path,
                                      const std::vector<ArtifactCore::KeyFrame>& keyframes) {
                const auto property = layer ? layer->getProperty(path) : nullptr;
                if (!property) return false;
                property->clearKeyFrames();
                for (const auto& keyframe : keyframes) {
                    property->addKeyFrame(
                        keyframe.time, keyframe.value, keyframe.interpolation,
                        keyframe.cp1_x, keyframe.cp1_y, keyframe.cp2_x,
                        keyframe.cp2_y, keyframe.roving);
                    property->setKeyFrameAnchorAt(keyframe.time, keyframe.anchor);
                    property->setKeyFrameColorLabelAt(keyframe.time,
                                                       keyframe.colorLabel);
                    property->setKeyFrameSoftAt(keyframe.time, keyframe.soft);
                }
                return true;
            };
            const auto restoreBefore = [&]() {
                const bool restored =
                    restoreProperty(writeLayer, QStringLiteral("transform.position.x"),
                                    beforePositionX) &&
                    restoreProperty(writeLayer, QStringLiteral("transform.position.y"),
                                    beforePositionY);
                const bool restoredAnchor = !options.writeAnchor ||
                    (restoreProperty(writeLayer, QStringLiteral("transform.anchor.x"),
                                     beforeAnchorX) &&
                     restoreProperty(writeLayer, QStringLiteral("transform.anchor.y"),
                                     beforeAnchorY));
                if (restored && restoredAnchor) writeLayer->changed();
                return restored && restoredAnchor;
            };
            const auto restoreAfter = [&]() {
                const bool restored =
                    restoreProperty(writeLayer, QStringLiteral("transform.position.x"),
                                    afterPositionX) &&
                    restoreProperty(writeLayer, QStringLiteral("transform.position.y"),
                                    afterPositionY);
                const bool restoredAnchor = !options.writeAnchor ||
                    (restoreProperty(writeLayer, QStringLiteral("transform.anchor.x"),
                                     afterAnchorX) &&
                     restoreProperty(writeLayer, QStringLiteral("transform.anchor.y"),
                                     afterAnchorY));
                if (restored && restoredAnchor) writeLayer->changed();
                return restored && restoredAnchor;
            };

            auto macro = std::make_unique<MacroUndoCommand>(
                QStringLiteral("Apply Tracking Result"));
            if (createdNullLayer) {
                macro->addChild(std::make_unique<AddLayerCommand>(
                    comp, writeLayer, true));
            }
            if (!sameKeyframes(beforePositionX, afterPositionX)) {
                macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                    writeLayer, QStringLiteral("transform.position.x"),
                    beforePositionX, afterPositionX));
            }
            if (!sameKeyframes(beforePositionY, afterPositionY)) {
                macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                    writeLayer, QStringLiteral("transform.position.y"),
                    beforePositionY, afterPositionY));
            }
            if (options.writeAnchor && !sameKeyframes(beforeAnchorX, afterAnchorX)) {
                macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                    writeLayer, QStringLiteral("transform.anchor.x"),
                    beforeAnchorX, afterAnchorX));
            }
            if (options.writeAnchor && !sameKeyframes(beforeAnchorY, afterAnchorY)) {
                macro->addChild(std::make_unique<SetLayerPropertyKeyframesCommand>(
                    writeLayer, QStringLiteral("transform.anchor.y"),
                    beforeAnchorY, afterAnchorY));
            }
            if (!restoreBefore()) return false;
            if (!undo->push(std::move(macro))) {
                restoreAfter();
                return false;
            }
        } else if (createdNullLayer) {
            const auto appendResult = comp->appendLayerTop(writeLayer);
            if (!appendResult.success) return false;
        }
        writeLayer->setDirty(LayerDirtyFlag::Transform);
        writeLayer->changed();

        return appliedAny;
    }

    /// 全トラッキングポイントの結果をそれぞれ個別の Null レイヤーに書き出す。
    static int applyAllTrackingPoints(
        const ArtifactCompositionPtr& comp,
        const ArtifactCore::MotionTracker& tracker,
        const ApplyOptions& options)
    {
        if (!comp) return 0;

        const auto result = tracker.result();
        if (result.frames.empty()) return 0;

        // 全ポイント ID を収集
        std::vector<int> pointIds;
        std::set<int> seenPointIds;
        for (const auto& frame : result.frames) {
            for (const auto& pt : frame.points) {
                if (pt.active) {
                    if (seenPointIds.insert(pt.id).second) {
                        pointIds.push_back(pt.id);
                        if (pointIds.size() >= 1024) {
                            break;
                        }
                    }
                }
            }
            if (pointIds.size() >= 1024) {
                break;
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

    /// Planar tracking の投影四隅を Corner Pin effect の animatable
    /// properties へ書き出す。既存の targetLayer に effect を追加し、
    /// source rect の各コーナーを時系列キーフレーム化する。
    static bool applyPlanarResultAsCornerPin(
        ArtifactAbstractComposition* comp,
        const ArtifactCore::MotionTracker& tracker,
        const QRectF& sourceRect,
        ArtifactAbstractLayerPtr targetLayer)
    {
        if (!comp || !targetLayer || tracker.trackerType() !=
            ArtifactCore::TrackerType::Planar || !sourceRect.isValid()) {
            return false;
        }
        const auto keyframes = tracker.exportProjectedRegionKeyframes(sourceRect);
        if (keyframes.empty()) return false;

        const double fpsValue = comp->frameRate().framerate();
        if (!std::isfinite(fpsValue) || fpsValue <= 0.0 || fpsValue > 240.0)
            return false;
        const auto compositionRange = comp->frameRange();
        if (!compositionRange.isValid()) return false;
        const auto fps = static_cast<int64_t>(std::llround(fpsValue));
        if (fps <= 0) return false;

        auto* effectService = ArtifactEffectService::instance();
        if (!effectService) return false;
        auto effect = effectService->createEffect(EffectID("builtin.corner_pin"));
        if (!effect) return false;
        auto effectPtr = ArtifactCore::makeShared(
            effect.release(), [](ArtifactAbstractEffect* pointer) { delete pointer; });
        if (!effectPtr) return false;
        const std::array<QString, 8> propertyNames = {
            QStringLiteral("Upper Left X"), QStringLiteral("Upper Left Y"),
            QStringLiteral("Upper Right X"), QStringLiteral("Upper Right Y"),
            QStringLiteral("Lower Left X"), QStringLiteral("Lower Left Y"),
            QStringLiteral("Lower Right X"), QStringLiteral("Lower Right Y")};
        std::array<ArtifactCore::SharedPtr<ArtifactCore::AbstractProperty>, 8> properties{};
        for (std::size_t i = 0; i < propertyNames.size(); ++i) {
            properties[i] = effectPtr->editableProperty(propertyNames[i]);
            if (!properties[i]) return false;
        }

        // Only attach the effect after all required properties have been
        // resolved. This keeps a failed export from leaving an unusable
        // partially-created Corner Pin effect on the target layer.
        std::array<std::vector<ArtifactCore::KeyFrame>, 8> generatedKeyframes{};
        for (const auto& [timeSeconds, corners] : keyframes) {
            if (!std::isfinite(timeSeconds) || std::abs(timeSeconds) > 1.0e9)
                continue;
            const auto frame = static_cast<int64_t>(std::llround(timeSeconds * fpsValue));
            if (frame < compositionRange.start() ||
                frame > compositionRange.end()) {
                continue;
            }
            const ArtifactCore::RationalTime time(frame, fps);
            const std::array<double, 8> values = {
                corners[0].x(), corners[0].y(), corners[1].x(), corners[1].y(),
                corners[2].x(), corners[2].y(), corners[3].x(), corners[3].y()};
            for (std::size_t i = 0; i < properties.size(); ++i) {
                if (!std::isfinite(values[i])) continue;
                generatedKeyframes[i].push_back(
                    ArtifactCore::KeyFrame{time, QVariant(values[i])});
            }
        }
        bool generatedAny = false;
        for (const auto& propertyKeyframes : generatedKeyframes) {
            if (!propertyKeyframes.empty()) {
                generatedAny = true;
                break;
            }
        }
        if (!generatedAny) return false;

        if (auto* undo = UndoManager::instance()) {
            auto macro = std::make_unique<MacroUndoCommand>(
                QStringLiteral("Apply Planar Corner Pin"));
            macro->addChild(std::make_unique<AddLayerEffectCommand>(
                targetLayer, effectPtr));
            for (std::size_t i = 0; i < properties.size(); ++i) {
                macro->addChild(std::make_unique<SetEffectPropertyKeyframesCommand>(
                    effectPtr, propertyNames[i],
                    std::vector<ArtifactCore::KeyFrame>{}, generatedKeyframes[i],
                    QStringLiteral("Apply Corner Pin Keyframes")));
            }
            if (!undo->push(std::move(macro))) return false;
        } else {
            targetLayer->addEffect(effectPtr);
            for (std::size_t i = 0; i < properties.size(); ++i) {
                for (const auto& keyframe : generatedKeyframes[i]) {
                    properties[i]->addKeyFrame(keyframe.time, keyframe.value);
                }
            }
        }
        targetLayer->setDirty(LayerDirtyFlag::Effect);
        targetLayer->changed();
        return true;
    }

    /// Planar tracking の homography を対象マスクのベジェ頂点へ適用し、
    /// フレームごとの MaskPath キーフレームを生成する（AE の Mask Tracking に相当）。
    ///
    /// sourceRect はトラッキング開始時のマスク外接矩形を渡す。 Corners は毎フレーム
    /// 4 隅が移動するため、平行移動だけでなく回転・スケール・スキューも反映される。
    /// 頂点数はフレーム間で一定なので、MaskPath の頂点列を保ったまま
    /// setAnimationKeyframe を積む。Undo は 1 コマンドにまとめる。
    static bool applyPlanarResultAsMask(
        ArtifactAbstractComposition* comp,
        const ArtifactCore::MotionTracker& tracker,
        const QRectF& sourceRect,
        ArtifactAbstractLayerPtr targetLayer,
        int maskIndex,
        int pathIndex)
    {
        if (!comp || !targetLayer || tracker.trackerType() !=
            ArtifactCore::TrackerType::Planar || !sourceRect.isValid()) {
            return false;
        }
        if (maskIndex < 0 || maskIndex >= targetLayer->maskCount()) {
            return false;
        }
        LayerMask mask = targetLayer->mask(maskIndex);
        if (pathIndex < 0 || pathIndex >= mask.maskPathCount()) {
            return false;
        }
        const MaskPath sourcePath = mask.maskPath(pathIndex);
        if (sourcePath.vertexCount() < 3) {
            return false;
        }

        const double fpsValue = comp->frameRate().framerate();
        if (!std::isfinite(fpsValue) || fpsValue <= 0.0 || fpsValue > 240.0) {
            return false;
        }
        const auto compositionRange = comp->frameRange();
        if (!compositionRange.isValid()) return false;

        // The quad is defined in layer-local space, which is also the space the
        // tracked corners arrive in, so one homography per frame maps the mask
        // vertices directly without any canvas round-trip.
        const std::array<QPointF, 4> sourceQuad = {
            QPointF(sourceRect.left(), sourceRect.top()),
            QPointF(sourceRect.right(), sourceRect.top()),
            QPointF(sourceRect.right(), sourceRect.bottom()),
            QPointF(sourceRect.left(), sourceRect.bottom())};

        // Solve every frame before mutating the layer: a failure part-way through
        // must not leave a half-tracked mask, and MaskPath would otherwise
        // interpolate straight across any skipped frame.
        const int vertexCount = sourcePath.vertexCount();
        std::vector<MaskPathKeyframeSnapshot> snapshots;
        snapshots.reserve(static_cast<size_t>(compositionRange.end() -
                                             compositionRange.start() + 1));
        for (int64_t frame = compositionRange.start();
             frame <= compositionRange.end(); ++frame) {
            const double timeSeconds = static_cast<double>(frame) / fpsValue;
            std::array<QPointF, 4> trackedQuad{};
            if (!tracker.projectRegionAt(timeSeconds, sourceRect, trackedQuad)) {
                continue;
            }
            double matrix[9] = {};
            if (!solveHomography4Point(sourceQuad, trackedQuad, matrix)) {
                continue;
            }

            MaskPathKeyframeSnapshot snapshot;
            snapshot.frame = frame;
            snapshot.closed = sourcePath.isClosed();
            snapshot.opacity = sourcePath.opacity();
            snapshot.feather = sourcePath.feather().value;
            snapshot.featherHorizontal = sourcePath.featherHorizontal().value;
            snapshot.featherVertical = sourcePath.featherVertical().value;
            snapshot.featherInner = sourcePath.featherInner().value;
            snapshot.featherOuter = sourcePath.featherOuter().value;
            snapshot.falloff = sourcePath.falloff();
            snapshot.expansion = sourcePath.expansion().value;
            snapshot.inverted = sourcePath.isInverted();
            snapshot.mode = sourcePath.mode();
            snapshot.name = sourcePath.name();
            snapshot.vertices.reserve(static_cast<size_t>(vertexCount));

            bool frameUsable = true;
            for (int v = 0; v < vertexCount && frameUsable; ++v) {
                const MaskVertex vertex = sourcePath.vertex(v);
                // Tangents are relative offsets, so they must be transformed as
                // direction vectors: map the anchor point and the absolute
                // tangent point, then difference them.  Transforming the offset
                // itself with a projective matrix would be wrong.
                const QPointF position = applyHomography3x3(matrix, vertex.position);
                const QPointF inAnchor =
                    applyHomography3x3(matrix, vertex.position + vertex.inTangent);
                const QPointF outAnchor =
                    applyHomography3x3(matrix, vertex.position + vertex.outTangent);
                if (!std::isfinite(position.x()) ||
                    !std::isfinite(position.y()) ||
                    !std::isfinite(inAnchor.x()) ||
                    !std::isfinite(inAnchor.y()) ||
                    !std::isfinite(outAnchor.x()) ||
                    !std::isfinite(outAnchor.y())) {
                    frameUsable = false;
                    break;
                }
                MaskVertex moved;
                moved.position = position;
                moved.inTangent = inAnchor - position;
                moved.outTangent = outAnchor - position;
                snapshot.vertices.push_back(moved);
            }
            if (frameUsable) {
                snapshots.push_back(std::move(snapshot));
            }
        }
        if (snapshots.size() < 2) {
            return false;
        }

        // Capture the full mask list before mutating so the Undo command can
        // restore every mask, matching how the interactive mask-edit
        // transaction behaves.  Kept as a copy because the rollback path below
        // still needs it after the command has consumed its own argument.
        std::vector<LayerMask> beforeMasks;
        beforeMasks.reserve(static_cast<size_t>(targetLayer->maskCount()));
        for (int i = 0; i < targetLayer->maskCount(); ++i) {
            beforeMasks.push_back(targetLayer->mask(i));
        }

        MaskPath tracked = sourcePath;
        tracked.clearAnimationKeyframes();
        for (const auto& snapshot : snapshots) {
            tracked.setAnimationKeyframe(snapshot.frame, snapshot);
        }
        mask.setMaskPath(pathIndex, tracked);
        targetLayer->setMask(maskIndex, mask);
        targetLayer->setDirty(LayerDirtyFlag::Mask);
        targetLayer->changed();

        std::vector<LayerMask> afterMasks;
        afterMasks.reserve(static_cast<size_t>(targetLayer->maskCount()));
        for (int i = 0; i < targetLayer->maskCount(); ++i) {
            afterMasks.push_back(targetLayer->mask(i));
        }
        if (auto* undo = UndoManager::instance()) {
            if (!undo->push(std::make_unique<MaskEditCommand>(
                    targetLayer, beforeMasks, afterMasks))) {
                // The undo stack refused the command: fail closed by restoring
                // the original mask rather than leaving an untracked mask that
                // the user cannot undo.
                for (int i = 0; i < beforeMasks.size(); ++i) {
                    targetLayer->setMask(i, beforeMasks[static_cast<size_t>(i)]);
                }
                targetLayer->setDirty(LayerDirtyFlag::Mask);
                targetLayer->changed();
                return false;
            }
        }
        return true;
    }
};

} // namespace Artifact
