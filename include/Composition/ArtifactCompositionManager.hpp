#pragma once

#include <QtCore/QObject>
#include <memory>
#include <vector>
#include "ArtifactAbstractComposition.ixx"
#include "ArtifactCompositionResult.ixx"
#include "ArtifactFindCompositionQuery.ixx"
#include "ArtifactCompositionInitParams.ixx"

namespace Artifact {

 class ArtifactCompositionManagerPrivate;

 class ArtifactCompositionManager : public QObject {
 private:
  std::unique_ptr<ArtifactCompositionManagerPrivate> d;

 public:
  explicit ArtifactCompositionManager(QObject* parent = nullptr);
  ~ArtifactCompositionManager();

  // Access
  size_t compositionCount() const;
  ArtifactCompositionPtr compositionAt(size_t index) const;
  ArtifactCompositionPtr compositionById(const CompositionID& id) const;
  std::vector<ArtifactCompositionPtr> allCompositions() const;
  std::vector<CompositionID> allCompositionIds() const;

  // Create / Remove
  CreateCompositionResult createNewComposition(const ArtifactCompositionInitParams& params = ArtifactCompositionInitParams::presetHD());
  RemoveAllCompositionResult removeAllCompositions();

  // Search
  std::vector<FindCompositionResult> search(const ArtifactFindCompositionQuery& query) const;
  void createComposition();
  void removeAllComposition();
 };

}
