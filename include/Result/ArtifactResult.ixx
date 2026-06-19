module;
#include <utility>

export module Artifact.Result;

import Utils.Result;
import Utils.String.UniString;

export namespace Artifact {

using namespace ArtifactCore;

struct CreateCompositionResult {
  bool success = false;
  Status status{};
  UniString message;
};

}
