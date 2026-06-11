module;
#include <memory>
#include <vector>
#include <unordered_map>
#include <wobjectdefs.h>
#include <QObject>

export module Artifact.Composition.Manager;

import Utils;
import Artifact.Composition.Abstract;
import Artifact.Composition.Result;

W_REGISTER_ARGTYPE(ArtifactCore::CompositionID)

export namespace Artifact {
using namespace ArtifactCore;

class ArtifactFindCompositionQuery;
class ArtifactCompositionInitParams;

class ArtifactCompositionManager : public QObject {
 W_OBJECT(ArtifactCompositionManager)
private:
 class Impl;
 std::unique_ptr<Impl> d_;

public:
 explicit ArtifactCompositionManager(QObject* parent = nullptr);
 ~ArtifactCompositionManager();

 size_t compositionCount() const;
 ArtifactCompositionPtr compositionAt(size_t index) const;
 ArtifactCompositionPtr compositionById(const CompositionID& id) const;
 std::vector<ArtifactCompositionPtr> allCompositions() const;
 std::vector<CompositionID> allCompositionIds() const;

 CreateCompositionResult createNewComposition();
 CreateCompositionResult createNewComposition(const ArtifactCompositionInitParams& params);
 RemoveAllCompositionResult removeAllCompositions();
 std::vector<FindCompositionResult> search(const ArtifactFindCompositionQuery& query) const;

 void createComposition();
 void removeAllComposition();

 void compositionCreated(const CompositionID& id, const ArtifactCompositionPtr& composition)
  W_SIGNAL(compositionCreated, id, composition);
 void compositionRemoved(const CompositionID& id)
  W_SIGNAL(compositionRemoved, id);
 void allCompositionsRemoved()
  W_SIGNAL(allCompositionsRemoved);
 void compositionChanged(const CompositionID& id)
  W_SIGNAL(compositionChanged, id);
};

}
