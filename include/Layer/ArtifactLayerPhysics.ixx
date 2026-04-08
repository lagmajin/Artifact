module;
#include <utility>
#include <QJsonObject>
export module Artifact.Layer.Physics;

export namespace Artifact {
    struct LayerPhysicsSettings {
        bool enabled = false;
        float stiffness = 120.0f;
        float damping = 12.0f;
        float followThroughGain = 0.5f;
        
        // Wiggle (Self-driven motion)
        float wiggleFreq = 0.0f; // Hz
        float wiggleAmp = 0.0f;  // px/deg/scale

        QJsonObject toJson() const {
            QJsonObject obj;
            obj["enabled"] = enabled;
            obj["stiffness"] = static_cast<double>(stiffness);
            obj["damping"] = static_cast<double>(damping);
            obj["followThroughGain"] = static_cast<double>(followThroughGain);
            obj["wiggleFreq"] = static_cast<double>(wiggleFreq);
            obj["wiggleAmp"] = static_cast<double>(wiggleAmp);
            return obj;
        }

        void fromJson(const QJsonObject& obj) {
            enabled = obj["enabled"].toBool(false);
            stiffness = static_cast<float>(obj["stiffness"].toDouble(120.0));
            damping = static_cast<float>(obj["damping"].toDouble(12.0));
            followThroughGain = static_cast<float>(obj["followThroughGain"].toDouble(0.5));
            wiggleFreq = static_cast<float>(obj["wiggleFreq"].toDouble(0.0));
            wiggleAmp = static_cast<float>(obj["wiggleAmp"].toDouble(0.0));
        }
    };
}
