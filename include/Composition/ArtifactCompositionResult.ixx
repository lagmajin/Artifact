module;
#include <utility>

export module Artifact.Composition.Result;

import Utils;
import Utils.Result;
import Utils.String.UniString;
import Memory.SharedPtr;

export namespace Artifact {

using namespace ArtifactCore;
class ArtifactAbstractComposition;

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

enum class AppendLayerToCompositionError {
  None = 0,
  CompositionNotFound,
  LayerNotFound,
  LayerTypeMismatch,
  UnknownError
};

struct AppendLayerToCompositionResult {
  bool success = false;
  Status status{};
  AppendLayerToCompositionError error = AppendLayerToCompositionError::None;
  UniString message;
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
