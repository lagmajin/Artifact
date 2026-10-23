module;

#include <memory>
#include <utility>
#include <vector>

module Artifact.Widgets.LayerEditor.MaskEditSession;

import Artifact.Layer.Abstract;
import Artifact.Mask.LayerMask;
import Undo.UndoManager;

namespace Artifact {

void LayerEditorMaskEditSession::begin(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer || (pending_ && layer_.lock() == layer)) return;
 pending_ = true;
 dirty_ = false;
 layer_ = layer;
 before_.clear();
 before_.reserve(static_cast<size_t>(layer->maskCount()));
 for (int index = 0; index < layer->maskCount(); ++index)
  before_.push_back(layer->mask(index));
}

void LayerEditorMaskEditSession::markDirty()
{
 if (pending_) dirty_ = true;
}

void LayerEditorMaskEditSession::commit()
{
 if (!pending_) return;
 auto layer = layer_.lock();
 pending_ = false;
 layer_.reset();
 if (!layer || !dirty_) {
  before_.clear();
  dirty_ = false;
  return;
 }
 std::vector<LayerMask> after;
 after.reserve(static_cast<size_t>(layer->maskCount()));
 for (int index = 0; index < layer->maskCount(); ++index)
  after.push_back(layer->mask(index));
 if (auto* undo = UndoManager::instance();
     undo && !undo->push(std::make_unique<MaskEditCommand>(layer, before_, std::move(after)))) {
  layer->clearMasks();
  for (const auto& mask : before_) layer->addMask(mask);
 }
 before_.clear();
 dirty_ = false;
}

void LayerEditorMaskEditSession::cancel()
{
 auto layer = layer_.lock();
 if (layer && pending_ && dirty_) {
  layer->clearMasks();
  for (const auto& mask : before_) layer->addMask(mask);
 }
 pending_ = false;
 dirty_ = false;
 layer_.reset();
 before_.clear();
}

bool LayerEditorMaskEditSession::pending() const noexcept
{
 return pending_;
}

}
