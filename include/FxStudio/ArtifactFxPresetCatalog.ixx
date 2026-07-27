module;

#include <vector>

export module Artifact.FxStudio.PresetCatalog;

import Artifact.FxStudio.Sequence;
import Utils.Optional;
import Core.ArtifactString;

export namespace Artifact::FxStudio {

struct PresetDescriptor {
  ArtifactCore::String id;
  ArtifactCore::String name;
  Family family = Family::Glitch;
  ArtifactCore::String description;
};

class PresetCatalog {
public:
  static const std::vector<PresetDescriptor>& descriptors();
  static const PresetDescriptor* find(const ArtifactCore::String& id);
  static ArtifactCore::Optional<Sequence> create(const ArtifactCore::String& id);
};

Sequence makeImpactPunchSequence();
Sequence makeRevealSweepSequence();
Sequence makeEnergySurgeSequence();
Sequence makeGlitchTransitionSequence();

}
