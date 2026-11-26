module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>

#include <QLineF>
#include <QMap>
#include <QPointF>
#include <QString>
#include <QVariant>
#include <QVector2D>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layers.Abstract._2D;
import Artifact.Preview.Pipeline;
import Artifact.Widgets.CompositionRigUndoCommands;
import ArtifactCore.Rig2D;
import Configuration.ConfigLayer;
import Configuration.LayeredConfigStore;
import Math.Vec;
import Memory.SharedPtr;
import Undo.UndoManager;
import Utils.Id;

namespace Artifact {

using namespace CompositionRenderControllerDetail;

void CompositionRenderController::setShowRigOverlay(bool show) {
  if (!impl_ || impl_->showRigOverlay_ == show) return;
  impl_->showRigOverlay_ = show;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/RigOverlayVisible"), show);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isShowRigOverlay() const {
  return impl_ ? impl_->showRigOverlay_ : false;
}

bool CompositionRenderController::clearRigSelection() {
  if (!impl_) return false;
  const bool hadSelection = !impl_->selectedRigBoneId_.isNil() ||
                            !impl_->selectedRigControlId_.isNil() ||
                            impl_->rigSelectionDragging_;
  impl_->selectedRigBoneId_ = {};
  impl_->selectedRigControlId_ = {};
  impl_->rigSelectionDragging_ = false;
  impl_->rigDragLayer_.reset();
  if (hadSelection) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
  return hadSelection;
}

bool CompositionRenderController::nudgeSelectedRigBoneRotation(
    ArtifactCore::Units::Degrees delta) {
  if (!impl_ || impl_->selectedRigBoneId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer) return false;
  auto *bone = rigLayer->rig2D().findBone(impl_->selectedRigBoneId_);
  if (!bone) return false;
  const auto before = bone->localTransform();
  auto after = before;
  after.rotation += delta.value;
  if (!rigLayer->setRigBoneLocalTransform(bone->id(), after)) return false;
  if (rigLayer->rig2D().rootBone()) rigLayer->rig2D().rootBone()->updateHierarchy();
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigBoneTransformUndoCommand>(
                                   layer, bone->id(), before, after));
  if (!pushed) {
    rigLayer->setRigBoneLocalTransform(bone->id(), before);
    if (rigLayer->rig2D().rootBone()) rigLayer->rig2D().rootBone()->updateHierarchy();
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

bool CompositionRenderController::nudgeSelectedRigControl(const QVector2D &delta) {
  if (!impl_ || impl_->selectedRigControlId_.isNil()) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer) return false;
  auto *control = rigLayer->rig2D().findControl(impl_->selectedRigControlId_);
  if (!control || control->kind() != RigControlKind::Point) return false;
  const QVariant before = control->value();
  const QVector2D current = before.value<QVector2D>();
  const QVariant after = QVariant::fromValue(current + delta);
  control->setValue(after);
  rigLayer->goToFrame(rigLayer->currentFrame());
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigControlValueUndoCommand>(
                                   layer, control->id(), before, after));
  if (!pushed) {
    control->setValue(before);
    rigLayer->goToFrame(rigLayer->currentFrame());
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

void CompositionRenderController::adjustRigWeightBrush(float radiusDelta,
                                                         float opacityDelta) {
  if (!impl_) return;
  impl_->rigWeightRadius_ = std::clamp(
      impl_->rigWeightRadius_ + radiusDelta, 2.0f, 500.0f);
  impl_->rigWeightOpacity_ = std::clamp(
      impl_->rigWeightOpacity_ + opacityDelta, 0.01f, 1.0f);
  auto& config = ArtifactCore::LayeredConfigStore::instance();
  config.setValue(QStringLiteral("Viewport/RigWeight/Radius"),
                  static_cast<double>(impl_->rigWeightRadius_));
  config.setValue(QStringLiteral("Viewport/RigWeight/Opacity"),
                  static_cast<double>(impl_->rigWeightOpacity_));
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

float CompositionRenderController::rigWeightBrushRadius() const {
  return impl_ ? impl_->rigWeightRadius_ : 36.0f;
}

float CompositionRenderController::rigWeightBrushOpacity() const {
  return impl_ ? impl_->rigWeightOpacity_ : 0.35f;
}

void CompositionRenderController::adjustRigWeightBrushFlow(float delta) {
  if (!impl_) return;
  impl_->rigWeightFlow_ = std::clamp(impl_->rigWeightFlow_ + delta, 0.01f, 1.0f);
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/RigWeight/Flow"),
      static_cast<double>(impl_->rigWeightFlow_));
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

float CompositionRenderController::rigWeightBrushFlow() const {
  return impl_ ? impl_->rigWeightFlow_ : 1.0f;
}

bool CompositionRenderController::normalizeRigWeights() {
  if (!impl_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer || !rigLayer->rig2D().skinMesh()) return false;
  auto *mesh = rigLayer->rig2D().skinMesh();
  const auto before = mesh->vertices();
  auto after = before;
  bool changed = false;
  for (auto &vertex : after) {
    float total = 0.0f;
    for (float weight : vertex.weights) total += std::max(0.0f, weight);
    if (total <= 0.0001f) continue;
    for (float &weight : vertex.weights) {
      const float next = std::clamp(weight, 0.0f, 1.0f) / total;
      changed = changed || !qFuzzyCompare(next + 1.0f, weight + 1.0f);
      weight = next;
    }
  }
  if (!changed) return false;
  mesh->setVertices(after);
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigSkinWeightsUndoCommand>(
                                   layer, before, after));
  if (!pushed) {
    mesh->setVertices(before);
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

bool CompositionRenderController::smoothRigWeights() {
  if (!impl_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer || !rigLayer->rig2D().skinMesh()) return false;
  auto *mesh = rigLayer->rig2D().skinMesh();
  const auto before = mesh->vertices();
  auto after = before;
  const int boneIndex = impl_->rigWeightBoneIndex_;
  const float radius = std::max(2.0f, impl_->rigWeightRadius_);
  bool changed = false;
  for (size_t i = 0; i < before.size(); ++i) {
    float ownWeight = 0.0f;
    for (int slot = 0; slot < 4; ++slot) {
      if (before[i].boneIndices[slot] == boneIndex) ownWeight = before[i].weights[slot];
    }
    float sum = ownWeight;
    int count = 1;
    for (size_t j = 0; j < before.size(); ++j) {
      if (i == j) continue;
      const QPointF a(before[i].position.x(), before[i].position.y());
      const QPointF b(before[j].position.x(), before[j].position.y());
      if (QLineF(a, b).length() > radius) continue;
      for (int slot = 0; slot < 4; ++slot) {
        if (before[j].boneIndices[slot] == boneIndex) {
          sum += before[j].weights[slot];
          ++count;
          break;
        }
      }
    }
    const float nextWeight = sum / static_cast<float>(count);
    for (int slot = 0; slot < 4; ++slot) {
      if (after[i].boneIndices[slot] == boneIndex) {
        changed = changed || !qFuzzyCompare(after[i].weights[slot] + 1.0f,
                                             nextWeight + 1.0f);
        after[i].weights[slot] = nextWeight;
      }
    }
  }
  if (!changed) return false;
  for (auto &vertex : after) {
    float total = 0.0f;
    for (float weight : vertex.weights) total += std::max(0.0f, weight);
    if (total > 0.0001f) {
      for (float &weight : vertex.weights) weight = std::max(0.0f, weight) / total;
    }
  }
  mesh->setVertices(after);
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigSkinWeightsUndoCommand>(
                                   layer, before, after));
  if (!pushed) {
    mesh->setVertices(before);
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

bool CompositionRenderController::mirrorRigWeights() {
  if (!impl_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer || !rigLayer->rig2D().skinMesh()) return false;
  auto *mesh = rigLayer->rig2D().skinMesh();
  const auto before = mesh->vertices();
  auto after = before;
  const int boneIndex = impl_->rigWeightBoneIndex_;
  const float mirrorTolerance = std::max(4.0f, impl_->rigWeightRadius_ * 2.0f);
  const float mirrorToleranceSquared = mirrorTolerance * mirrorTolerance;
  bool changed = false;
  for (size_t i = 0; i < before.size(); ++i) {
    float bestDistance = std::numeric_limits<float>::max();
    size_t bestIndex = i;
    const QVector2D target(-before[i].position.x(), before[i].position.y());
    for (size_t j = 0; j < before.size(); ++j) {
      const float distance = (before[j].position - target).lengthSquared();
      if (distance < bestDistance) {
        bestDistance = distance;
        bestIndex = j;
      }
    }
    if (bestDistance > mirrorToleranceSquared) continue;
    float mirroredWeight = 0.0f;
    for (int slot = 0; slot < 4; ++slot) {
      if (before[bestIndex].boneIndices[slot] == boneIndex) {
        mirroredWeight = before[bestIndex].weights[slot];
        break;
      }
    }
    int targetSlot = -1;
    for (int slot = 0; slot < 4; ++slot) {
      if (after[i].boneIndices[slot] == boneIndex) {
        targetSlot = slot;
        break;
      }
    }
    if (targetSlot < 0 && mirroredWeight > 0.0001f) {
      targetSlot = 0;
      for (int slot = 1; slot < 4; ++slot) {
        if (after[i].weights[slot] < after[i].weights[targetSlot]) {
          targetSlot = slot;
        }
      }
      after[i].boneIndices[targetSlot] = static_cast<int16_t>(boneIndex);
      after[i].weights[targetSlot] = 0.0f;
    }
    if (targetSlot >= 0) {
      changed = changed || !qFuzzyCompare(after[i].weights[targetSlot] + 1.0f,
                                           mirroredWeight + 1.0f);
      after[i].weights[targetSlot] = mirroredWeight;
    }
  }
  if (!changed) return false;
  for (auto &vertex : after) {
    float total = 0.0f;
    for (float weight : vertex.weights) total += std::max(0.0f, weight);
    if (total > 0.0001f) {
      for (float &weight : vertex.weights) weight = std::max(0.0f, weight) / total;
    }
  }
  mesh->setVertices(after);
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigSkinWeightsUndoCommand>(
                                   layer, before, after));
  if (!pushed) {
    mesh->setVertices(before);
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

bool CompositionRenderController::captureRigPose() {
  if (!impl_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer) return false;
  impl_->rigPoseClipboard_ = ArtifactCore::capturePose(rigLayer->rig2D());
  impl_->rigPoseClipboardValid_ = true;
  return true;
}

bool CompositionRenderController::applyCapturedRigPose(float blendWeight) {
  if (!impl_ || !impl_->rigPoseClipboardValid_) return false;
  const auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp && !impl_->selectedLayerId_.isNil()
      ? comp->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *rigLayer = layer
      ? dynamic_cast<ArtifactAbstract2DLayer *>(layer.get())
      : nullptr;
  if (!rigLayer) return false;
  const auto before = ArtifactCore::capturePose(rigLayer->rig2D());
  const float blend = std::clamp(blendWeight, 0.0f, 1.0f);
  ArtifactCore::PoseSnapshot target = blend >= 1.0f
      ? impl_->rigPoseClipboard_
      : ArtifactCore::blendPoses(before, impl_->rigPoseClipboard_, blend);
  if (blend < 1.0f) {
    for (const auto &[id, value] : impl_->rigPoseClipboard_.controlValues) {
      const auto currentIt = before.controlValues.find(id);
      if (currentIt == before.controlValues.end()) continue;
      if (value.canConvert<QVector2D>() && currentIt->second.canConvert<QVector2D>()) {
        const QVector2D from = currentIt->second.value<QVector2D>();
        const QVector2D to = value.value<QVector2D>();
        target.controlValues[id] = QVariant::fromValue(from + (to - from) * blend);
      } else if (!value.canConvert<double>() ||
                 !currentIt->second.canConvert<double>()) {
        target.controlValues[id] = blend >= 0.5f ? value : currentIt->second;
      }
    }
  }
  ArtifactCore::applyPose(rigLayer->rig2D(), target);
  if (rigLayer->rig2D().rootBone()) rigLayer->rig2D().rootBone()->updateHierarchy();
  const auto after = ArtifactCore::capturePose(rigLayer->rig2D());
  rigLayer->setDirty(LayerDirtyFlag::Transform);
  rigLayer->changed();
  auto *manager = UndoManager::instance();
  const bool pushed =
      !manager || manager->push(std::make_unique<RigPoseUndoCommand>(
                                   layer, before, after));
  if (!pushed) {
    ArtifactCore::applyPose(rigLayer->rig2D(), before);
    if (rigLayer->rig2D().rootBone()) rigLayer->rig2D().rootBone()->updateHierarchy();
    rigLayer->setDirty(LayerDirtyFlag::Transform);
    rigLayer->changed();
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return false;
  }
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  impl_->publishLayerModified(layer, true);
  markRenderDirty();
  return true;
}

bool CompositionRenderController::saveRigPoseSlot(int slot) {
  if (!impl_ || slot < 1 || slot > 9) return false;
  if (!captureRigPose()) return false;
  ArtifactCore::LayeredConfigStore::instance().setValue(
      QStringLiteral("Viewport/RigPose/Slot%1").arg(slot),
      rigPoseToVariantMap(impl_->rigPoseClipboard_));
  return true;
}

bool CompositionRenderController::applyRigPoseSlot(int slot, float blendWeight) {
  if (!impl_ || slot < 1 || slot > 9) return false;
  const QString key = QStringLiteral("Viewport/RigPose/Slot%1").arg(slot);
  const QVariant stored = ArtifactCore::LayeredConfigStore::instance().value(key);
  if (!stored.isValid() || stored.toMap().isEmpty()) return false;
  impl_->rigPoseClipboard_ = rigPoseFromVariantMap(
      stored.toMap());
  impl_->rigPoseClipboardValid_ = true;
  return applyCapturedRigPose(blendWeight);
}

void CompositionRenderController::clearRigPoseSlots() {
  auto& config = ArtifactCore::LayeredConfigStore::instance();
  for (int slot = 1; slot <= 9; ++slot) {
    const auto key = QStringLiteral("Viewport/RigPose/Slot%1").arg(slot);
    config.removeValue(ArtifactCore::ConfigLayer::Project, key.toStdString());
    config.removeValue(ArtifactCore::ConfigLayer::User, key.toStdString());
  }
  if (impl_) {
    impl_->rigPoseClipboard_ = ArtifactCore::PoseSnapshot{};
    impl_->rigPoseClipboardValid_ = false;
    impl_->invalidateOverlayComposite();
    markRenderDirty();
  }
}

} // namespace Artifact
