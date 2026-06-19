module;
#include <utility>

export module Artifact.Project.Result;

import Utils.Result;
import Utils.String.UniString;

export namespace Artifact {

enum eCreateProjectError {
  None = 0,
  Unknown,
  InvalidName,
  DuplicateName,
  NotFound,
  Failed
};

struct CreateProjectResult {
  bool isSuccess = false;
  Status status{};
  UniString message;
};

struct CloseProjectResult {
  bool isSuccess = false;
  Status status{};
  UniString message;
};

struct ProjectToJsonResult {
  bool isSuccess = false;
  Status status{};
  UniString message;
};

}
