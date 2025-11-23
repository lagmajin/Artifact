module ;
#include <QSet>
export module Artifact.Layers.Selection.Manager;

import std;
import Artifact.Layers.Abstract;

export namespace Artifact {

 class ArtifactLayerSelectionManager {
 private:
  class Impl;
  Impl* impl_;
 public:
  ArtifactLayerSelectionManager();
  ~ArtifactLayerSelectionManager();
  void selectLayer(const ArtifactAbstractLayerPtr& layer);
  void addToSelection(const ArtifactAbstractLayerPtr& layer);
  void removeFromSelection(const ArtifactAbstractLayerPtr& layer);
  void clearSelection();
  bool isSelected(const ArtifactAbstractLayerPtr& layer) const;
  QSet<ArtifactAbstractLayerPtr> selectedLayers() const;
 };


}