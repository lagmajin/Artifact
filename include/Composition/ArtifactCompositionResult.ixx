module;
#include <utility>
#include <memory>
#include <QString>
#include <QVector>

export module Artifact.Composition.Result;


import Utils;
import Utils.String.UniString;

export namespace Artifact {

 using namespace ArtifactCore;
 class ArtifactAbstractComposition;

 struct CreateCompositionResult {
  CompositionID id{};
  bool success{false};
  UniString message;
 };

 struct ChangeCompositionResult {
  bool success = false;
  UniString message;
 };

 struct FindCompositionResult {
  bool success = false;
  std::weak_ptr<ArtifactAbstractComposition> ptr;
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
  AppendLayerToCompositionError error = AppendLayerToCompositionError::None;
  UniString message;
 };

 struct AllCompositionResult {
  bool success = false;
 };

 struct RemoveAllCompositionResult {
  bool success = false;
 };

}
