module;
#include <QString>
#include <QVector>
export module Artifact.Composition.Result;

import Utils;
import Utils.String.UniString;

import Artifact.Composition.Abstract;

export namespace Artifact {

 using namespace ArtifactCore;

 struct CreateCompositionResult {
  CompositionID id{};
  //ArtifactCompositionPtr composition;
  bool success{ false };
  UniString message;
 };

 struct ChangeCompositionResult {
  bool success = false;
  UniString message;

 };

 struct FindCompositionResult
 {
  bool success = false;
  ArtifactCompositionWeakPtr ptr;
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

 struct AllCompositionResult
 {
  bool success = false;
 	
 };


};
