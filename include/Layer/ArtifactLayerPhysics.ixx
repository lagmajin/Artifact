module;
#include <cstdint>
#include <cmath>
#include <utility>
#include <QJsonObject>
export module Artifact.Layer.Physics;

import Animation.Value;

export namespace Artifact {
    struct LayerPhysicsSettings {
        bool enabled = false;
        float stiffness = 120.0f;
        float damping = 12.0f;
        float followThroughGain = 0.5f;
        float gravityY = 980.0f; // px/s^2, positive Y falls downward in composition space.
        float linearDamping = 0.0f;
        
        // Wiggle (Self-driven motion)
        float wiggleFreq = 0.0f; // Hz
        float wiggleAmp = 0.0f;  // px/deg/scale

        QJsonObject toJson() const {
            QJsonObject obj;
            obj["enabled"] = enabled;
            obj["stiffness"] = static_cast<double>(stiffness);
            obj["damping"] = static_cast<double>(damping);
            obj["followThroughGain"] = static_cast<double>(followThroughGain);
            obj["gravityY"] = static_cast<double>(gravityY);
            obj["linearDamping"] = static_cast<double>(linearDamping);
            obj["wiggleFreq"] = static_cast<double>(wiggleFreq);
            obj["wiggleAmp"] = static_cast<double>(wiggleAmp);
            return obj;
        }

        void fromJson(const QJsonObject& obj) {
            enabled = obj["enabled"].toBool(false);
            stiffness = static_cast<float>(obj["stiffness"].toDouble(120.0));
            damping = static_cast<float>(obj["damping"].toDouble(12.0));
            followThroughGain = static_cast<float>(obj["followThroughGain"].toDouble(0.5));
            gravityY = static_cast<float>(obj["gravityY"].toDouble(980.0));
            linearDamping = static_cast<float>(obj["linearDamping"].toDouble(0.0));
            wiggleFreq = static_cast<float>(obj["wiggleFreq"].toDouble(0.0));
            wiggleAmp = static_cast<float>(obj["wiggleAmp"].toDouble(0.0));
        }
    };

    struct LayerPhysicsFrameInput {
        double positionX = 0.0;
        double positionY = 0.0;
        double rotation = 0.0;
        double previousPositionX = 0.0;
        double previousPositionY = 0.0;
        double previousRotation = 0.0;
        double frameTimeSeconds = 0.0;
        double framesPerSecond = 30.0;
        std::int64_t frame = 0;
    };

    struct LayerPhysicsFrameOutput {
        double positionX = 0.0;
        double positionY = 0.0;
        double rotation = 0.0;
    };

    class PhysicsLayerComponent {
    public:
        LayerPhysicsSettings& settings() { return settings_; }
        const LayerPhysicsSettings& settings() const { return settings_; }

        bool enabled() const { return settings_.enabled; }
        void setEnabled(bool enabled) {
            if (settings_.enabled == enabled) {
                return;
            }
            settings_.enabled = enabled;
            reset();
        }

        void reset() const {
            springX_ = {};
            springY_ = {};
            springRot_ = {};
            dynamicOffsetY_ = 0.0f;
            dynamicVelocityY_ = 0.0f;
            lastFrame_ = -1;
        }

        LayerPhysicsFrameOutput apply(const LayerPhysicsFrameInput& input) const {
            LayerPhysicsFrameOutput output{input.positionX, input.positionY, input.rotation};
            if (!settings_.enabled) {
                reset();
                return output;
            }

            const double fps = input.framesPerSecond > 0.0 ? input.framesPerSecond : 30.0;
            const float dt = 1.0f / static_cast<float>(fps);
            const std::int64_t curFrame = input.frame;

            if (lastFrame_ == -1 || curFrame < lastFrame_ || (curFrame - lastFrame_) > 10) {
                resetSprings();
            } else if (curFrame == lastFrame_) {
                output.positionX += springX_.currentValue;
                output.positionY += springY_.currentValue + dynamicOffsetY_;
                output.rotation += springRot_.currentValue;
                return output;
            } else {
                const float velocityX = static_cast<float>(input.positionX - input.previousPositionX) / dt;
                const float velocityY = static_cast<float>(input.positionY - input.previousPositionY) / dt;
                const float velocityRot = static_cast<float>(input.rotation - input.previousRotation) / dt;
                const std::int64_t steps = curFrame - lastFrame_;
                const float currentSec = static_cast<float>(input.frameTimeSeconds);

                for (std::int64_t i = 0; i < steps; ++i) {
                    const float stepTime = currentSec + (static_cast<float>(i) * dt);
                    updateChannel(springX_, velocityX, 0.0f, stepTime, dt);
                    updateChannel(springY_, velocityY, 1.0f, stepTime, dt);
                    updateChannel(springRot_, velocityRot, 2.0f, stepTime, dt);
                    updateGravity(dt);
                }
            }

            lastFrame_ = curFrame;
            output.positionX += springX_.currentValue;
            output.positionY += springY_.currentValue + dynamicOffsetY_;
            output.rotation += springRot_.currentValue;
            return output;
        }

    private:
        void resetSprings() const {
            springX_.currentValue = 0.0f;
            springX_.velocity = 0.0f;
            springY_.currentValue = 0.0f;
            springY_.velocity = 0.0f;
            springRot_.currentValue = 0.0f;
            springRot_.velocity = 0.0f;
            springX_.initialized = true;
            springY_.initialized = true;
            springRot_.initialized = true;
            dynamicOffsetY_ = 0.0f;
            dynamicVelocityY_ = 0.0f;
        }

        void updateGravity(float dt) const {
            if (std::abs(settings_.gravityY) <= 0.01f) {
                return;
            }
            dynamicVelocityY_ += settings_.gravityY * dt;
            if (settings_.linearDamping > 0.0f) {
                const float dampingFactor = 1.0f / (1.0f + settings_.linearDamping * dt);
                dynamicVelocityY_ *= dampingFactor;
            }
            dynamicOffsetY_ += dynamicVelocityY_ * dt;
        }

        void updateChannel(ArtifactCore::SpringState& state,
                           float baseVelocity,
                           float channelIndex,
                           float stepTime,
                           float dt) const {
            state.stiffness = settings_.stiffness;
            state.damping = settings_.damping;
            state.velocity -= baseVelocity * settings_.followThroughGain;

            float wiggleForce = 0.0f;
            if (settings_.wiggleFreq > 0.01f && settings_.wiggleAmp > 0.01f) {
                const float phase = channelIndex * 1.57f;
                const float noise =
                    std::sin(stepTime * settings_.wiggleFreq * 6.28f + phase) +
                    std::sin(stepTime * settings_.wiggleFreq * 1.33f * 6.28f + phase * 0.5f) * 0.5f;
                wiggleForce = noise * settings_.wiggleAmp * state.stiffness * 0.1f;
            }

            const float force =
                -state.stiffness * state.currentValue - state.damping * state.velocity + wiggleForce;
            state.velocity += force * dt;
            state.currentValue += state.velocity * dt;
        }

        LayerPhysicsSettings settings_;
        mutable ArtifactCore::SpringState springX_;
        mutable ArtifactCore::SpringState springY_;
        mutable ArtifactCore::SpringState springRot_;
        mutable float dynamicOffsetY_ = 0.0f;
        mutable float dynamicVelocityY_ = 0.0f;
        mutable std::int64_t lastFrame_ = -1;
    };
}
