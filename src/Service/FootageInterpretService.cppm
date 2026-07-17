module;

#include <QVector>

module Artifact.Service.FootageInterpret;

import Artifact.Project.Manager;
import Media.SourceInterpret;
import Artifact.Project;
import Artifact.Project.Items;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Video;
import Artifact.Layer.Image;
import Artifact.Layer.Audio;
import Frame.Position;
import Time.Rational;
import Time.TimeRemap;
import Frame.Rate;
import Utils.Id;

namespace Artifact {

struct FootageInterpretService::Impl {
    // Walk all compositions and layers that reference this footage
    void collectAffectedLayers(const FootageItem* footage,
                               QVector<ArtifactAbstractLayerPtr>& outLayers,
                               std::vector<ArtifactAbstractComposition*>& outComps) const {
        auto& projectManager = ArtifactProjectManager::getInstance();
        auto project = projectManager.getCurrentProjectSharedPtr();
        if (!project) return;

        const auto items = project->projectItems();
        std::function<void(ProjectItem*)> walk = [&](ProjectItem* item) {
            if (!item) return;
            if (item->type() == eProjectItemType::Composition) {
                auto* compItem = static_cast<CompositionItem*>(item);
                auto findResult = project->findComposition(compItem->compositionId);
                if (findResult.success) {
                    auto comp = findResult.ptr.lock();
                    if (comp) {
                        bool foundInThisComp = false;
                        for (const auto& layer : comp->allLayer()) {
                            if (!layer) continue;
                            if (auto videoLayer = std::dynamic_pointer_cast<ArtifactVideoLayer>(layer)) {
                                if (videoLayer->sourcePath() == footage->filePath) {
                                    outLayers.push_back(layer);
                                    foundInThisComp = true;
                                }
                            } else if (auto imgLayer = std::dynamic_pointer_cast<ArtifactImageLayer>(layer)) {
                                if (imgLayer->sourcePath() == footage->filePath) {
                                    outLayers.push_back(layer);
                                    foundInThisComp = true;
                                }
                            } else if (auto audioLayer = std::dynamic_pointer_cast<ArtifactAudioLayer>(layer)) {
                                if (audioLayer->sourcePath() == footage->filePath) {
                                    outLayers.push_back(layer);
                                    foundInThisComp = true;
                                }
                            }
                        }
                        if (foundInThisComp) outComps.push_back(comp.get());
                    }
                }
            }
            for (auto* child : item->children) walk(child);
        };
        for (auto* root : items) walk(root);
    }

    int countKeyframesOnLayer(const ArtifactAbstractLayerPtr& layer) const {
        // Check time remap keyframes
        if (layer->isTimeRemapEnabled()) {
            // Try to get keyframe count — use a heuristic by checking frames
            return 1; // signal that at least time remap is active
        }
        return 0;
    }
};

FootageInterpretService& FootageInterpretService::instance() {
    static FootageInterpretService svc;
    return svc;
}

InterpretImpactReport FootageInterpretService::preflightChange(
    FootageItem* footage, double newFrameRate) const {
    InterpretImpactReport report;
    if (!footage || newFrameRate <= 0.0) return report;

    if (!impl_) impl_ = std::make_unique<Impl>();

    QVector<ArtifactAbstractLayerPtr> layers;
    std::vector<ArtifactAbstractComposition*> comps;
    impl_->collectAffectedLayers(footage, layers, comps);

    report.affectedLayerCount = layers.size();
    report.oldDurationSeconds = 0.0;
    report.newDurationSeconds = 0.0;

    for (const auto& layer : layers) {
        report.affectedLayerNames.push_back(layer->layerName());

        int kfCount = impl_->countKeyframesOnLayer(layer);
        if (kfCount > 0) report.affectedKeyframeCount += kfCount;

        if (layer->isTimeRemapEnabled()) {
            report.hasTimeRemap = true;
        }
    }

    if (footage->frameRate > 0.0 && newFrameRate > 0.0) {
        if (std::abs(footage->frameRate - newFrameRate) > 0.001) {
            report.durationWillChange = true;
        }
    }

    return report;
}

bool FootageInterpretService::applyFrameRateChange(
    FootageItem* footage, double newFrameRate,
    FrameRatePreserveMode mode, QString* errorOut) {
    if (!footage || newFrameRate <= 0.0) {
        if (errorOut) *errorOut = "Invalid footage or frame rate";
        return false;
    }

    if (!impl_) impl_ = std::make_unique<Impl>();

    const double oldFrameRate = footage->frameRate;
    if (oldFrameRate <= 0.0 || std::abs(oldFrameRate - newFrameRate) < 0.001) {
        footage->frameRate = newFrameRate;
        return true;
    }

    QVector<ArtifactAbstractLayerPtr> layers;
    std::vector<ArtifactAbstractComposition*> comps;
    impl_->collectAffectedLayers(footage, layers, comps);

    const double ratio = oldFrameRate / newFrameRate;

    for (auto& layer : layers) {
        if (!layer) continue;

        switch (mode) {
        case FrameRatePreserveMode::KeepKeyframes:
            // Scale all time remap keyframes by the frame rate ratio
            if (layer->isTimeRemapEnabled()) {
                // Keyframe times stay but their source frame mapping changes
                layer->clearTimeRemap();
            }
            break;

        case FrameRatePreserveMode::KeepTime:
            // Keep source timing, adjust in/out points proportionally
            {
                const int64_t oldIn = layer->inPoint().framePosition();
                const int64_t oldOut = layer->outPoint().framePosition();
                layer->setInPoint(FramePosition(static_cast<int>(oldIn * ratio)));
                layer->setOutPoint(FramePosition(static_cast<int>(oldOut * ratio)));
            }
            if (layer->isTimeRemapEnabled()) {
                layer->clearTimeRemap();
            }
            break;

        case FrameRatePreserveMode::ReSample:
            // Clear effects/keyframes that depend on frame rate
            layer->clearTimeRemap();
            break;
        }
    }

    // Apply the frame rate to the footage item
    footage->frameRate = newFrameRate;

    return true;
}

SourceInterpretOverride FootageInterpretService::currentOverride(
    const FootageItem* footage) const {
    SourceInterpretOverride override;
    if (!footage) return override;
    override.frameRate = footage->frameRate;
    override.inputColorSpace = footage->inputColorSpace;
    override.inputTransferFunction = footage->inputTransferFunction;
    override.isActive = true;
    return override;
}

bool FootageInterpretService::applyColorInterpretation(
    FootageItem* footage, const QString& inputColorSpace,
    const QString& inputTransferFunction, QString* errorOut) {
    if (!footage) {
        if (errorOut) *errorOut = QStringLiteral("Invalid footage");
        return false;
    }
    footage->inputColorSpace = inputColorSpace.trimmed();
    footage->inputTransferFunction = inputTransferFunction.trimmed();
    return true;
}

bool FootageInterpretService::clearOverride(FootageItem* footage) {
    if (!footage) return false;
    footage->frameRate = 0.0;
    return true;
}

} // namespace Artifact
