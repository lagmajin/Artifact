#include "../../include/Composition/ArtifactCompositionManager.hpp"

import Artifact.Composition._2D;

namespace Artifact {

 class ArtifactCompositionManagerPrivate {
 public:
  std::vector<ArtifactCompositionPtr> compositions_;
  std::unordered_map<QString, size_t> idIndex_;
 };

 ArtifactCompositionManager::ArtifactCompositionManager(QObject* parent)
  : QObject(parent), d(std::make_unique<ArtifactCompositionManagerPrivate>())
 {
 }

 ArtifactCompositionManager::~ArtifactCompositionManager() = default;

 size_t ArtifactCompositionManager::compositionCount() const
 {
  return d->compositions_.size();
 }

 ArtifactCompositionPtr ArtifactCompositionManager::compositionAt(size_t index) const
 {
  if (index >= d->compositions_.size()) return nullptr;
  return d->compositions_[index];
 }

 ArtifactCompositionPtr ArtifactCompositionManager::compositionById(const CompositionID& id) const
 {
  const auto it = d->idIndex_.find(id.toQString());
  if (it == d->idIndex_.end() || it->second >= d->compositions_.size()) return nullptr;
  return d->compositions_[it->second];
 }

 std::vector<ArtifactCompositionPtr> ArtifactCompositionManager::allCompositions() const
 {
  return d->compositions_;
 }

 std::vector<CompositionID> ArtifactCompositionManager::allCompositionIds() const
 {
  std::vector<CompositionID> ids;
  ids.reserve(d->compositions_.size());
  for (const auto& comp : d->compositions_) {
   ids.push_back(comp->id());
  }
  return ids;
 }

 CreateCompositionResult ArtifactCompositionManager::createNewComposition(const ArtifactCompositionInitParams& params)
 {
  CreateCompositionResult result;
  try {
   const CompositionID id;
   auto comp = std::make_shared<ArtifactComposition2D>(id, params);
   if (!comp) {
    result.message = UniString::fromUtf8("Failed to allocate composition");
    return result;
   }
   d->idIndex_[id.toQString()] = d->compositions_.size();
   d->compositions_.push_back(comp);
   result.id = id;
   result.success = true;
  } catch (const std::exception& e) {
   result.message = UniString::fromUtf8(e.what());
  }
  return result;
 }

 RemoveAllCompositionResult ArtifactCompositionManager::removeAllCompositions()
 {
 RemoveAllCompositionResult result;
  d->compositions_.clear();
  d->idIndex_.clear();
  result.success = true;
  return result;
 }

 std::vector<FindCompositionResult> ArtifactCompositionManager::search(const ArtifactFindCompositionQuery& query) const
 {
  std::vector<FindCompositionResult> results;
  for (const auto& comp : d->compositions_) {
   if (query.matches(comp)) {
    FindCompositionResult fr;
    fr.success = true;
    fr.ptr = comp;
    results.push_back(fr);
   }
  }
  return results;
 }

 void ArtifactCompositionManager::createComposition()
 {
  createNewComposition(ArtifactCompositionInitParams::presetHD());
 }

 void ArtifactCompositionManager::removeAllComposition()
 {
  removeAllCompositions();
 }

}
