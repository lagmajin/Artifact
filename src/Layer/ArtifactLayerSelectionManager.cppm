module;
#include <QSet>
#include <QVector>
#include <wobjectimpl.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Layers.Selection.Manager;




import Artifact.Layer.Abstract;
import Artifact.Composition.Abstract;

namespace Artifact {

 class ArtifactLayerSelectionManager::Impl {
 public:
  QSet<ArtifactAbstractLayerPtr> selectedLayers_;
  QVector<ArtifactAbstractLayerPtr> selectedLayerOrder_;
  ArtifactAbstractLayerPtr currentLayer_;
  ArtifactCompositionPtr activeComp_;
 };

 W_OBJECT_IMPL(ArtifactLayerSelectionManager)

 ArtifactLayerSelectionManager::ArtifactLayerSelectionManager(QObject* parent) 
  : QObject(parent), impl_(new Impl())
 {
 }

 ArtifactLayerSelectionManager* ArtifactLayerSelectionManager::instance() {
  static ArtifactLayerSelectionManager manager;
  return &manager;
 }

 ArtifactLayerSelectionManager::~ArtifactLayerSelectionManager() {
  delete impl_;
 }

 void ArtifactLayerSelectionManager::selectLayer(const ArtifactAbstractLayerPtr& layer) {
  if (layer && (layer->isLocked() || layer->isSelectionLocked())) {
   return;
  }
  const bool sameSingleSelection =
      impl_->selectedLayers_.size() == 1 && impl_->selectedLayers_.contains(layer);
  if (!sameSingleSelection) {
   impl_->selectedLayers_.clear();
   impl_->selectedLayerOrder_.clear();
   if (layer) {
    impl_->selectedLayers_.insert(layer);
    impl_->selectedLayerOrder_.push_back(layer);
   }
  }
  const bool currentChanged = impl_->currentLayer_ != layer;
  if (layer) {
   impl_->currentLayer_ = layer;
  } else {
   impl_->currentLayer_.reset();
  }
  if (!sameSingleSelection || currentChanged) {
   selectionChanged();
  }
 }

 void ArtifactLayerSelectionManager::addToSelection(const ArtifactAbstractLayerPtr& layer) {
  if (!layer || layer->isLocked() || layer->isSelectionLocked()) return;
  const bool currentChanged = impl_->currentLayer_ != layer;
  bool changed = false;
  if (!impl_->selectedLayers_.contains(layer)) {
   impl_->selectedLayers_.insert(layer);
   impl_->selectedLayerOrder_.push_back(layer);
   changed = true;
  }
  impl_->currentLayer_ = layer;
  if (changed || currentChanged) {
   selectionChanged();
  }
 }

 void ArtifactLayerSelectionManager::removeFromSelection(const ArtifactAbstractLayerPtr& layer) {
  if (!layer) return;
  if (impl_->selectedLayers_.remove(layer)) {
   impl_->selectedLayerOrder_.erase(
       std::remove(impl_->selectedLayerOrder_.begin(),
                   impl_->selectedLayerOrder_.end(), layer),
       impl_->selectedLayerOrder_.end());
   if (impl_->currentLayer_ == layer) {
    impl_->currentLayer_ = impl_->selectedLayerOrder_.isEmpty()
                               ? ArtifactAbstractLayerPtr{}
                               : impl_->selectedLayerOrder_.last();
   }
   selectionChanged();
  }
 }

 void ArtifactLayerSelectionManager::clearSelection() {
  if (!impl_->selectedLayers_.isEmpty()) {
   impl_->selectedLayers_.clear();
   impl_->selectedLayerOrder_.clear();
   impl_->currentLayer_.reset();
   selectionChanged();
  }
 }

 bool ArtifactLayerSelectionManager::isSelected(const ArtifactAbstractLayerPtr& layer) const {
  return impl_->selectedLayers_.contains(layer);
 }

 QSet<ArtifactAbstractLayerPtr> ArtifactLayerSelectionManager::selectedLayers() const {
  return impl_->selectedLayers_;
 }

 QVector<ArtifactAbstractLayerPtr> ArtifactLayerSelectionManager::selectedLayersInOrder() const {
  return impl_->selectedLayerOrder_;
 }

 ArtifactAbstractLayerPtr ArtifactLayerSelectionManager::currentLayer() const {
  return impl_->currentLayer_ ? impl_->currentLayer_
                              : (impl_->selectedLayerOrder_.isEmpty()
                                     ? ArtifactAbstractLayerPtr{}
                                     : impl_->selectedLayerOrder_.last());
 }

 void ArtifactLayerSelectionManager::setActiveComposition(const ArtifactCompositionPtr& comp) {
  if (impl_->activeComp_ == comp) return;
  impl_->activeComp_ = comp;
  activeCompositionChanged();
 }

 ArtifactCompositionPtr ArtifactLayerSelectionManager::activeComposition() const {
  return impl_->activeComp_;
 }

};
