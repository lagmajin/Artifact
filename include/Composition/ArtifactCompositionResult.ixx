module;
#include <QString>

export module Artifact.Composition.Result;

import Utils;

export namespace Artifact {

 using namespace ArtifactCore;

 struct CompositionResult {
  CompositionID id{};
  //ArtifactCompositionPtr composition;
  bool success{ false };
  QString errorMessage;
 };



};
