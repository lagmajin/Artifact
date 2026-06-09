#pragma once

#include <QtCore/QObject>
#include <memory>
#include <vector>
#include "Artifact/include/Composition/ArtifactAbstractComposition.ixx"
#include "Artifact/include/Composition/ArtifactCompositionResult.ixx"
#include "Artifact/include/Composition/ArtifactFindCompositionQuery.ixx"
#include "Artifact/include/Composition/ArtifactCompositionInitParams.ixx"

namespace Artifact {

 class ArtifactCompositionManagerPrivate;

 class ArtifactCompositionManager : public QObject {
  Q_OBJECT
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

 signals:
  void compositionCreated(const CompositionID& id, const ArtifactCompositionPtr& composition);
  void compositionRemoved(const CompositionID& id);
  void allCompositionsRemoved();
  void compositionChanged(const CompositionID& id);

 public slots:
  void createComposition();
  void removeAllComposition();
 };

}
