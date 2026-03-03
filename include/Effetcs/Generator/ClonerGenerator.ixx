module;
#include <QString>

export module Artifact.Effect.Generator.Cloner;

import std;
import Artifact.Effect.Abstract;
import Utils.String.UniString;

export namespace Artifact {

    using namespace ArtifactCore;

    class ClonerGenerator : public ArtifactAbstractEffect {
    public:
        ClonerGenerator() {
            setDisplayName(ArtifactCore::UniString("Cloner (Generator)"));
            setPipelineStage(EffectPipelineStage::Generator);
        }
        virtual ~ClonerGenerator() = default;

        // Add properties for Grid size, Count, etc...
    };

}
