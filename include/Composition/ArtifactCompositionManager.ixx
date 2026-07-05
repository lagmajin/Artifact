module;
#include <memory>
#include <vector>
#include <unordered_map>
#include <QObject>

export module Artifact.Composition.Manager;

import Utils;
import Artifact.Composition.Abstract;
import Artifact.Composition.InitParams;
import Artifact.Composition.Result;

export namespace Artifact {

class ArtifactFindCompositionQuery;

class ArtifactCompositionManager : public QObject {
private:
 class Impl;
 std::unique_ptr<Impl> d_;

public:
 explicit ArtifactCompositionManager(QObject* parent = nullptr);
 ~ArtifactCompositionManager();

 size_t compositionCount() const;
 ArtifactCompositionPtr compositionAt(size_t index) const;
 ArtifactCompositionPtr compositionById(const ArtifactCore::CompositionID& id) const;
 std::vector<ArtifactCompositionPtr> allCompositions() const;
 std::vector<ArtifactCore::CompositionID> allCompositionIds() const;

 CreateCompositionResult createNewComposition();
 CreateCompositionResult createNewComposition(const ArtifactCompositionInitParams& params);
 RemoveAllCompositionResult removeAllCompositions();
 std::vector<FindCompositionResult> search(const ArtifactFindCompositionQuery& query) const;

 void createComposition();
 void removeAllComposition();
};

}
