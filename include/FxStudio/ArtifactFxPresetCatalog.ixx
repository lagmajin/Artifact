module;

#include <optional>
#include <string_view>
#include <vector>

export module Artifact.FxStudio.PresetCatalog;

import Artifact.FxStudio.Sequence;

export namespace Artifact::FxStudio {

struct PresetDescriptor {
  std::string_view id;
  std::string_view name;
  Family family = Family::Glitch;
  std::string_view description;
};

class PresetCatalog {
public:
  static const std::vector<PresetDescriptor>& descriptors();
  static const PresetDescriptor* find(std::string_view id);
  static std::optional<Sequence> create(std::string_view id);
};

Sequence makeImpactPunchSequence();
Sequence makeRevealSweepSequence();
Sequence makeEnergySurgeSequence();
Sequence makeGlitchTransitionSequence();

}
