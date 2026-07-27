module;
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <wobjectdefs.h>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QRandomGenerator>

export module Artifact.Reactive.Executor;

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

export namespace Artifact {

/**
 * @brief Executes ReactiveRule reactions when TriggerEvents fire.
 *
 * Receives fired events from ReactiveEngine::evaluate() and dispatches
 * the corresponding reaction (SetProperty, GoToFrame, SpawnLayer, etc.).
 */
class ArtifactReactionExecutor : public QObject {
    W_OBJECT(ArtifactReactionExecutor)

public:
    explicit ArtifactReactionExecutor(QObject* parent = nullptr);
    ~ArtifactReactionExecutor();

    void setComposition(ArtifactCompositionPtr comp);
    ArtifactCompositionPtr composition() const;

    void setPlaybackController(ArtifactCompositionPlaybackController* ctrl);

    /// Execute a single reaction. Called by the layer integration.
    void executeReaction(const ArtifactCore::Reaction& reaction, const ArtifactCore::TriggerEvent& trigger);

    /// Execute all reactions from a rule. Convenience wrapper.
    void executeRule(const ArtifactCore::ReactiveRule& rule, const ArtifactCore::TriggerEvent& trigger);

    // Signals
    void reactionExecuted(const QString& ruleId, const QString& reactionType)
        W_SIGNAL(reactionExecuted, ruleId, reactionType);
    void executionError(const QString& ruleId, const QString& message)
        W_SIGNAL(executionError, ruleId, message);

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
