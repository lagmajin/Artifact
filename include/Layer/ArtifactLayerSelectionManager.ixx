module ;
#include <QSet>
#include <QObject>
#include <wobjectdefs.h>

export module Artifact.Layers.Selection.Manager;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

export namespace Artifact {

 class ArtifactLayerSelectionManager : public QObject {
   W_OBJECT(ArtifactLayerSelectionManager)
 private:
  class Impl;
  Impl* impl_;
 public:
  explicit ArtifactLayerSelectionManager(QObject* parent = nullptr);
  ~ArtifactLayerSelectionManager();

  // Layer Selection
  void selectLayer(const ArtifactAbstractLayerPtr& layer);
  void addToSelection(const ArtifactAbstractLayerPtr& layer);
  void removeFromSelection(const ArtifactAbstractLayerPtr& layer);
  void clearSelection();
  bool isSelected(const ArtifactAbstractLayerPtr& layer) const;
  QSet<ArtifactAbstractLayerPtr> selectedLayers() const;
  ArtifactAbstractLayerPtr currentLayer() const;

  // Composition Context
  void setActiveComposition(const ArtifactCompositionPtr& comp);
  ArtifactCompositionPtr activeComposition() const;

  void selectionChanged() W_SIGNAL(selectionChanged);
  void activeCompositionChanged(const ArtifactCompositionPtr& comp) W_SIGNAL(activeCompositionChanged, comp);
 };

}