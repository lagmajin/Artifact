module;
#include <utility>

export module Artifact.Asset.Result;

import Utils.Result;
import Utils.String.UniString;

export namespace Artifact {

struct FindAssetResult {
  bool success = false;
  Status status{};
  UniString message;
};

struct RemoveAssetResult {
  bool success = false;
  Status status{};
  UniString message;
};

}
