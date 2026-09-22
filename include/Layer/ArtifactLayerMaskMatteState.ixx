module;
#include <cstdint>
#include <vector>

export module Artifact.Layer.MaskMatteState;

import Artifact.Layer.Matte;
import Artifact.Mask.LayerMask;
import Container.NamedVector;

export namespace Artifact {

using namespace ArtifactCore;

class LayerMaskMatteState {
public:
  void addMask(const LayerMask &mask);
  void removeMask(int index);
  bool moveMask(int fromIndex, int toIndex);
  void setMask(int index, const LayerMask &mask);
  LayerMask mask(int index) const;
  int maskCount() const;
  void clearMasks();
  std::vector<LayerMask> masks() const;
  void setMasks(const std::vector<LayerMask> &masks);
  std::uint64_t maskRevision() const noexcept;

  std::vector<LayerMatteReference> matteReferences() const;
  void setMatteReferences(const std::vector<LayerMatteReference> &refs);
  void addMatteReference(const LayerMatteReference &ref);
  void clearMatteReferences();
  const NamedVector<LayerMatteReference> &mattes() const noexcept;

private:
  NamedVector<LayerMask> masks_{ContainerName{"Layer.Masks"}};
  std::uint64_t maskRevision_ = 0;
  NamedVector<LayerMatteReference> mattes_{
      ContainerName{"Layer.MatteReferences"}};
};

} // namespace Artifact
