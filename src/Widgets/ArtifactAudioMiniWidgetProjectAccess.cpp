module;

#include <new>
#include <utility>

module Artifact.Widgets.AudioMiniWidget;

import Utils.Id;
import Artifact.Composition.Abstract;
import Artifact.Composition.InOutPoints;
import Artifact.Service.Project;

namespace Artifact {

void* acquireAudioMiniWidgetComposition(const ArtifactCore::CompositionID& compositionId) {
  auto* service = ArtifactProjectService::instance();
  if (!service) return nullptr;
  const auto result = service->findComposition(compositionId);
  auto composition = result.success ? result.ptr.lock() : ArtifactCompositionPtr{};
  if (!composition) return nullptr;
  // The widget keeps this small owner handle between refreshes so the
  // composition remains alive for audio inspection and marker editing.
  return new (std::nothrow) ArtifactCompositionPtr(std::move(composition));
}

void releaseAudioMiniWidgetComposition(void* compositionHandle) {
  delete static_cast<ArtifactCompositionPtr*>(compositionHandle);
}

void* audioMiniWidgetCompositionAddress(void* compositionHandle) {
  const auto* composition = static_cast<const ArtifactCompositionPtr*>(compositionHandle);
  return composition ? composition->get() : nullptr;
}

ArtifactInOutPoints* audioMiniWidgetInOutPoints(void* compositionHandle) {
  const auto* composition = static_cast<const ArtifactCompositionPtr*>(compositionHandle);
  return composition && *composition ? (*composition)->inOutPoints() : nullptr;
}

} // namespace Artifact
