module;
#include <QSet>
module Artifact.Layers.Selection.Manager;

import std;
import Artifact.Layer.Abstract;


namespace Artifact {

 class ArtifactLayerSelectionManager::Impl {
 private:

 public:
  Impl();
  ~Impl();
  QSet<ArtifactAbstractLayerPtr> selectedLayers_;
 };

 ArtifactLayerSelectionManager::Impl::Impl()
 {

 }

 ArtifactLayerSelectionManager::Impl::~Impl()
 {

 }

 ArtifactLayerSelectionManager::ArtifactLayerSelectionManager() :impl_(new Impl())
 {

 }

 ArtifactLayerSelectionManager::~ArtifactLayerSelectionManager()
 {
  delete impl_;
 }

 void ArtifactLayerSelectionManager::selectLayer(const ArtifactAbstractLayerPtr& layer)
 {
  impl_->selectedLayers_.clear();
  if (layer) impl_->selectedLayers_.insert(layer);
  //emit selectionChanged();
 }

 void ArtifactLayerSelectionManager::addToSelection(const ArtifactAbstractLayerPtr& layer)
 {
  if (!layer) return;
  if (!impl_->selectedLayers_.contains(layer)) {
   impl_->selectedLayers_.insert(layer);
   //emit selectionChanged();
  }
 }

 void ArtifactLayerSelectionManager::removeFromSelection(const ArtifactAbstractLayerPtr& layer)
 {

 }

 void ArtifactLayerSelectionManager::clearSelection()
 {
  if (!impl_->selectedLayers_.isEmpty()) {
   impl_->selectedLayers_.clear();
   //emit selectionChanged();
  }
 }

};