module;

export module Composition3D;

import std;

export namespace Artifact {

 enum eCompositionType {
  OriginalComposition,
  PreCompose,
 };

 class ArtifactCompositionPrivate;

 class ArtifactComposition3D {
 private: 
  std::unique_ptr<ArtifactCompositionPrivate> pImpl_;
 public:
  ArtifactComposition3D(const ArtifactComposition3D& composition);
  ArtifactComposition3D(ArtifactComposition3D&& composition);
  virtual ~ArtifactComposition3D();

 };

}