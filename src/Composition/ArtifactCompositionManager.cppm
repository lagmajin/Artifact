module;
#include <exception>
#include <wobjectimpl.h>

module Artifact.Composition.Manager;

import Artifact.Composition._2D;
import Artifact.Composition.FindQuery;
import Artifact.Composition.InitParams;
import Utils.String.UniString;

namespace Artifact {

class ArtifactCompositionManager::Impl {
public:
 std::vector<ArtifactCompositionPtr> compositions_;
 std::unordered_map<QString, size_t> idIndex_;
};

W_OBJECT_IMPL(ArtifactCompositionManager)

ArtifactCompositionManager::ArtifactCompositionManager(QObject* parent)
 : QObject(parent), d_(std::make_unique<Impl>())
{
}

ArtifactCompositionManager::~ArtifactCompositionManager() = default;

size_t ArtifactCompositionManager::compositionCount() const
{
 return d_->compositions_.size();
}

ArtifactCompositionPtr ArtifactCompositionManager::compositionAt(size_t index) const
{
 if (index >= d_->compositions_.size()) {
  return nullptr;
 }
 return d_->compositions_[index];
}

ArtifactCompositionPtr ArtifactCompositionManager::compositionById(const CompositionID& id) const
{
 const auto it = d_->idIndex_.find(id.toQString());
 if (it == d_->idIndex_.end() || it->second >= d_->compositions_.size()) {
  return nullptr;
 }
 return d_->compositions_[it->second];
}

std::vector<ArtifactCompositionPtr> ArtifactCompositionManager::allCompositions() const
{
 return d_->compositions_;
}

std::vector<CompositionID> ArtifactCompositionManager::allCompositionIds() const
{
 std::vector<CompositionID> ids;
 ids.reserve(d_->compositions_.size());
 for (const auto& comp : d_->compositions_) {
  ids.push_back(comp->id());
 }
 return ids;
}

CreateCompositionResult ArtifactCompositionManager::createNewComposition()
{
 return createNewComposition(ArtifactCompositionInitParams::hdPreset());
}

CreateCompositionResult ArtifactCompositionManager::createNewComposition(
   const ArtifactCompositionInitParams& params)
{
 CreateCompositionResult result;
 try {
  const CompositionID id;
  auto comp = std::make_shared<ArtifactComposition2D>(id, params);
  if (!comp) {
   result.message = UniString::fromUtf8("Failed to allocate composition");
   return result;
  }
  d_->idIndex_[id.toQString()] = d_->compositions_.size();
  d_->compositions_.push_back(comp);
  result.id = id;
  result.success = true;
  compositionCreated(id, comp);
 } catch (const std::exception& e) {
  result.message = UniString::fromUtf8(e.what());
 }
 return result;
}

RemoveAllCompositionResult ArtifactCompositionManager::removeAllCompositions()
{
 RemoveAllCompositionResult result;
 d_->compositions_.clear();
 d_->idIndex_.clear();
 result.success = true;
 allCompositionsRemoved();
 return result;
}

std::vector<FindCompositionResult> ArtifactCompositionManager::search(
    const ArtifactFindCompositionQuery& query) const
{
 std::vector<FindCompositionResult> results;
 for (const auto& comp : d_->compositions_) {
  if (!comp) {
   continue;
  }

  const auto settings = comp->settings();
  const QSize size = comp->effectiveCompositionSize();
  const double fps = comp->frameRate().framerate();
  const double duration = fps > 0.0
      ? static_cast<double>(comp->frameRange().duration()) / fps
      : 0.0;

  if (query.matches(settings.compositionName().toQString(),
                    size.width(),
                    size.height(),
                    fps,
                    duration)) {
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
 createNewComposition(ArtifactCompositionInitParams::hdPreset());
}

void ArtifactCompositionManager::removeAllComposition()
{
 removeAllCompositions();
}

}
