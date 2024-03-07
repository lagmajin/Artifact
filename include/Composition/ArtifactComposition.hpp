#pragma once

#include <memory>


namespace Artifact {

 enum eCompositionType {
  OriginalComposition,
  PreCompose,
 };

 class ArtifactCompositionPrivate;

 class ArtifactComposition {
 private: 
  std::unique_ptr<ArtifactCompositionPrivate> pImpl_;
 public:
  ArtifactComposition(const ArtifactComposition& composition);
  ArtifactComposition(ArtifactComposition&& composition);
  virtual ~ArtifactComposition();

 };

}