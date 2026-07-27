module;
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wobjectimpl.h>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QRandomGenerator>

module Artifact.Reactive.Executor;

import Reactive.Events;
import Artifact.Layer.Abstract;
import Artifact.Layer.Factory;
import Artifact.Layer.InitParams;
import Artifact.Composition.Abstract;
import Artifact.Composition.PlaybackController;
import Frame.Position;
import Frame.Rate;
import Property.Abstract;
import Animation.Value;

namespace Artifact {

struct ArtifactReactionExecutor::Impl {
    std::shared_ptr<ArtifactAbstractComposition> composition;
    ArtifactCompositionPlaybackController* playbackController = nullptr;

    ArtifactAbstractLayerPtr findLayer(const QString& layerId) const {
        if (!composition) return nullptr;
        return composition->layerById(LayerID(layerId));
    }

    void handleSetProperty(const ArtifactCore::Reaction& r) {
        auto layer = findLayer(r.targetLayerId);
        if (!layer) return;
        layer->setLayerPropertyValue(r.propertyPath, r.value);
    }

    void handleAnimateProperty(const ArtifactCore::Reaction& r) {
        auto layer = findLayer(r.targetLayerId);
        if (!layer) return;
        auto prop = layer->getProperty(r.propertyPath);
        if (!prop || !prop->isAnimatable()) return;
        // Add keyframe at target frame (use current frame + 1 for "immediate next frame"
        // or derive from duration if we had fps context — the caller should set targetFrame)
        int64_t frame = composition ? composition->framePosition().framePosition() + 1 : 1;
        InterpolationType interp = InterpolationType::Linear;
        prop->addKeyFrame(ArtifactCore::RationalTime(frame, 30), r.value);
    }

    void handleRandomizeProperty(const ArtifactCore::Reaction& r) {
        auto layer = findLayer(r.targetLayerId);
        if (!layer) return;
        double minVal = r.value.toDouble();
        double maxVal = r.valueMax.toDouble();
        double random = minVal + (maxVal - minVal) * QRandomGenerator::global()->generateDouble();
        layer->setLayerPropertyValue(r.propertyPath, QVariant(random));
    }

    void handlePlayAnimation(const ArtifactCore::Reaction&) {
        if (playbackController) playbackController->play();
    }

    void handlePauseAnimation(const ArtifactCore::Reaction&) {
        if (playbackController) playbackController->pause();
    }

    void handleGoToFrame(const ArtifactCore::Reaction& r) {
        if (playbackController)
            playbackController->goToFrame(FramePosition(r.targetFrame));
    }

    void handleSpawnLayer(const ArtifactCore::Reaction& r) {
        if (!composition) return;
        ArtifactLayerFactory factory;
        ArtifactLayerInitParams params(r.spawnLayerType, LayerType::Solid);
        auto layer = factory.createNewLayer(params);
        if (layer)
            composition->appendLayerTop(layer);
    }

    void handleDestroyLayer(const ArtifactCore::Reaction& r) {
        if (!composition) return;
        composition->removeLayerById(LayerID(r.targetLayerId));
    }
};

W_OBJECT_IMPL(ArtifactReactionExecutor)

ArtifactReactionExecutor::ArtifactReactionExecutor(QObject* parent)
    : QObject(parent), impl_(new Impl()) {}

ArtifactReactionExecutor::~ArtifactReactionExecutor() { delete impl_; }

void ArtifactReactionExecutor::setComposition(const std::shared_ptr<ArtifactAbstractComposition>& comp) {
    impl_->composition = comp;
}

std::shared_ptr<ArtifactAbstractComposition> ArtifactReactionExecutor::composition() const {
    return impl_->composition;
}

void ArtifactReactionExecutor::setPlaybackController(ArtifactCompositionPlaybackController* ctrl) {
    impl_->playbackController = ctrl;
}

void ArtifactReactionExecutor::executeRule(const ArtifactCore::ReactiveRule& rule, const ArtifactCore::TriggerEvent& trigger) {
    for (const auto& reaction : rule.reactions) {
        executeReaction(reaction, trigger);
    }
}

void ArtifactReactionExecutor::executeReaction(const ArtifactCore::Reaction& reaction, const ArtifactCore::TriggerEvent& trigger) {
    switch (reaction.type) {
    case ArtifactCore::ReactionType::SetProperty:
        impl_->handleSetProperty(reaction);
        break;
    case ArtifactCore::ReactionType::AnimateProperty:
        impl_->handleAnimateProperty(reaction);
        break;
    case ArtifactCore::ReactionType::RandomizeProperty:
        impl_->handleRandomizeProperty(reaction);
        break;
    case ArtifactCore::ReactionType::PlayAnimation:
        impl_->handlePlayAnimation(reaction);
        break;
    case ArtifactCore::ReactionType::PauseAnimation:
        impl_->handlePauseAnimation(reaction);
        break;
    case ArtifactCore::ReactionType::GoToFrame:
        impl_->handleGoToFrame(reaction);
        break;
    case ArtifactCore::ReactionType::SpawnLayer:
        impl_->handleSpawnLayer(reaction);
        break;
    case ArtifactCore::ReactionType::DestroyLayer:
        impl_->handleDestroyLayer(reaction);
        break;
    default:
        // ApplyImpulse, ApplyForce, Attract, Repel, PlaySound — TBD
        break;
    }
    Q_EMIT reactionExecuted(trigger.ruleId, QString::number(static_cast<int>(reaction.type)));
}

} // namespace Artifact
