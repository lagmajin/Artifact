module;
#include <utility>

export module Artifact.Composition.Result;

import Utils;
import Utils.Result;
import Utils.String.UniString;
import Memory.SharedPtr;
export import Artifact.Composition.Abstract;

export namespace Artifact {

using namespace ArtifactCore;

struct CreateCompositionResult {
  CompositionID id{};
  bool success{false};
  Status status{};
  UniString message;
};

struct ChangeCompositionResult {
  bool success = false;
  Status status{};
  UniString message;
};

struct FindCompositionResult {
  bool success = false;
  Status status{};
  WeakPtr<ArtifactAbstractComposition> ptr;
};

struct AllCompositionResult {
  bool success = false;
  Status status{};
};

struct RemoveAllCompositionResult {
  bool success = false;
  Status status{};
};

}
