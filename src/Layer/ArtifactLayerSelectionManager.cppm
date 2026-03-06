module;
#include <QSet>
#include <wobjectimpl.h>
module Artifact.Layers.Selection.Manager;

import std;
import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;


namespace Artifact {

 class ArtifactLayerSelectionManager::Impl {
 public:
  QSet<ArtifactAbstractLayerPtr> selectedLayers_;
  ArtifactCompositionPtr activeComp_;
 };

 W_OBJECT_IMPL(ArtifactLayerSelectionManager)

 ArtifactLayerSelectionManager::ArtifactLayerSelectionManager(QObject* parent) 
  : QObject(parent), impl_(new Impl())
 {
 }

 ArtifactLayerSelectionManager::~ArtifactLayerSelectionManager() {
  delete impl_;
 }

 void ArtifactLayerSelectionManager::selectLayer(const ArtifactAbstractLayerPtr& layer) {
  if (impl_->selectedLayers_.size() == 1 && impl_->selectedLayers_.contains(layer)) return;
  impl_->selectedLayers_.clear();
  if (layer) impl_->selectedLayers_.insert(layer);
  selectionChanged();
 }

 void ArtifactLayerSelectionManager::addToSelection(const ArtifactAbstractLayerPtr& layer) {
  if (!layer) return;
  if (!impl_->selectedLayers_.contains(layer)) {
   impl_->selectedLayers_.insert(layer);
   selectionChanged();
  }
 }

 void ArtifactLayerSelectionManager::removeFromSelection(const ArtifactAbstractLayerPtr& layer) {
  if (!layer) return;
  if (impl_->selectedLayers_.remove(layer)) selectionChanged();
 }

 void ArtifactLayerSelectionManager::clearSelection() {
  if (!impl_->selectedLayers_.isEmpty()) {
   impl_->selectedLayers_.clear();
   selectionChanged();
  }
 }

 bool ArtifactLayerSelectionManager::isSelected(const ArtifactAbstractLayerPtr& layer) const {
  return impl_->selectedLayers_.contains(layer);
 }

 QSet<ArtifactAbstractLayerPtr> ArtifactLayerSelectionManager::selectedLayers() const {
  return impl_->selectedLayers_;
 }

 ArtifactAbstractLayerPtr ArtifactLayerSelectionManager::currentLayer() const {
  if (impl_->selectedLayers_.isEmpty()) return nullptr;
  return *impl_->selectedLayers_.begin();
 }

 void ArtifactLayerSelectionManager::setActiveComposition(const ArtifactCompositionPtr& comp) {
  if (impl_->activeComp_ == comp) return;
  impl_->activeComp_ = comp;
  activeCompositionChanged(comp);
 }

 ArtifactCompositionPtr ArtifactLayerSelectionManager::activeComposition() const {
  return impl_->activeComp_;
 }

};