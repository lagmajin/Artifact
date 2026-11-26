module;

#define QT_NO_KEYWORDS

#include <cstdint>

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <opencv2/core.hpp>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import :RasterEffects;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Layer.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Paint;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Preview.Pipeline;
import Artifact.Tool.Brush;
import Artifact.Tool.Manager;
import ArtifactCore.ImageProcessing.OpenCV.RotoBrushEngine;
import Memory.SharedPtr;
import Utils.Id;

namespace Artifact {

using namespace CompositionRenderControllerDetail;

bool CompositionRenderController::cancelBrushStroke() {
  if (!impl_) {
    return false;
  }
  const auto comp = impl_->previewPipeline_.composition();
  auto layer = comp ? comp->layerById(impl_->selectedLayerId_) : nullptr;
  auto *brushTool = ArtifactApplicationManager::instance()
                        ? ArtifactApplicationManager::instance()->brushTool()
                        : nullptr;
  if (!layer || !brushTool || !brushTool->isDragging()) {
    return false;
  }
  brushTool->cancelStroke(layer);
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::propagateRotoBrushToCurrentFrame() {
  if (!impl_) return false;
  const auto composition = impl_->previewPipeline_.composition();
  const auto layer = composition
      ? composition->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *imageLayer = layer ? dynamic_cast<ArtifactImageLayer *>(layer.get()) : nullptr;
  if (!imageLayer || !impl_->rotoBrushEngine_ || impl_->rotoBrushSource_.empty() ||
      impl_->rotoBrushMaskIndex_ < 0 ||
      impl_->rotoBrushMaskIndex_ >= layer->maskCount()) {
    return false;
  }
  const int64_t frame = static_cast<int64_t>(layer->currentFrame());
  if (frame == impl_->rotoBrushFrame_) return false;
  const QImage current = imageLayer->toQImage();
  if (current.isNull()) return false;
  cv::Mat currentMat = compositionQImageToCvMat(current, true);
  if (currentMat.empty() || currentMat.size() != impl_->rotoBrushSource_.size()) {
    return false;
  }
  impl_->rotoBrushEngine_->propagateToNextFrame(
      &impl_->rotoBrushSource_, &currentMat);
  impl_->rotoBrushEngine_->refineCurrentMask(1);
  impl_->rotoBrushMask_ = static_cast<const cv::Mat*>(
      impl_->rotoBrushEngine_->currentMask())->clone();
  const QRectF bounds = imageLayer->localBounds();
  if (bounds.width() <= 0.0 || bounds.height() <= 0.0) return false;
  LayerMask generatedMask;
  auto paths = MaskPath::fromAlphaMask(&impl_->rotoBrushMask_);
  for (auto &path : paths) {
    for (int i = 0; i < path.vertexCount(); ++i) {
      auto vertex = path.vertex(i);
      vertex.position = QPointF(bounds.left() + vertex.position.x() * bounds.width(),
                                bounds.top() + vertex.position.y() * bounds.height());
      vertex.inTangent = QPointF(vertex.inTangent.x() * bounds.width(),
                                 vertex.inTangent.y() * bounds.height());
      vertex.outTangent = QPointF(vertex.outTangent.x() * bounds.width(),
                                  vertex.outTangent.y() * bounds.height());
      path.setVertex(i, vertex);
    }
    generatedMask.addMaskPath(path);
  }
  if (generatedMask.maskPathCount() == 0) return false;
  layer->setMask(impl_->rotoBrushMaskIndex_, generatedMask);
  impl_->rotoBrushSource_ = currentMat.clone();
  impl_->rotoBrushFrame_ = frame;
  layer->changed();
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  return true;
}

bool CompositionRenderController::undoSelectedPaintStroke() {
  if (!impl_) {
    return false;
  }
  const auto composition = impl_->previewPipeline_.composition();
  const auto layer = composition
                         ? composition->layerById(impl_->selectedLayerId_)
                         : ArtifactAbstractLayerPtr{};
  auto *toolManager = ArtifactApplicationManager::instance()
                          ? ArtifactApplicationManager::instance()->toolManager()
                          : nullptr;
  if (toolManager && toolManager->activeTool() == ToolType::RotoBrush &&
      layer && impl_->rotoBrushLayerId_ == layer->id() &&
      impl_->rotoBrushMaskIndex_ >= 0 &&
      impl_->rotoBrushMaskIndex_ < layer->maskCount()) {
    if (!dynamic_cast<ArtifactImageLayer *>(layer.get())) return false;
    impl_->beginMaskEditTransaction(layer);
    if (!impl_->rotoBrushStrokes_.empty()) {
      impl_->rotoBrushStrokes_.pop_back();
    }
    if (impl_->rotoBrushStrokes_.empty() || impl_->rotoBrushSource_.empty()) {
      layer->removeMask(impl_->rotoBrushMaskIndex_);
      impl_->rotoBrushMaskIndex_ = -1;
      impl_->rotoBrushMask_.release();
      impl_->rotoBrushSource_.release();
      if (impl_->rotoBrushEngine_) impl_->rotoBrushEngine_->reset();
    } else if (impl_->rotoBrushEngine_) {
      impl_->rotoBrushEngine_->updateBaseFrame(
          &impl_->rotoBrushSource_, impl_->rotoBrushStrokes_);
      impl_->rotoBrushEngine_->refineCurrentMask(1);
      impl_->rotoBrushMask_ = static_cast<const cv::Mat*>(
          impl_->rotoBrushEngine_->currentMask())->clone();
      const auto *imageLayer = dynamic_cast<ArtifactImageLayer *>(layer.get());
      const QRectF bounds = imageLayer ? imageLayer->localBounds() : QRectF();
      auto paths = MaskPath::fromAlphaMask(&impl_->rotoBrushMask_);
      LayerMask generatedMask;
      for (auto &path : paths) {
        for (int i = 0; i < path.vertexCount(); ++i) {
          auto vertex = path.vertex(i);
          vertex.position = QPointF(bounds.left() + vertex.position.x() * bounds.width(),
                                    bounds.top() + vertex.position.y() * bounds.height());
          vertex.inTangent = QPointF(vertex.inTangent.x() * bounds.width(),
                                     vertex.inTangent.y() * bounds.height());
          vertex.outTangent = QPointF(vertex.outTangent.x() * bounds.width(),
                                      vertex.outTangent.y() * bounds.height());
          path.setVertex(i, vertex);
        }
        generatedMask.addMaskPath(path);
      }
      if (generatedMask.maskPathCount() > 0) {
        layer->setMask(impl_->rotoBrushMaskIndex_, generatedMask);
      } else {
        // A valid undo can leave no foreground pixels. Remove the old
        // generated mask instead of keeping a stale contour on the layer.
        layer->removeMask(impl_->rotoBrushMaskIndex_);
        impl_->rotoBrushMaskIndex_ = -1;
        impl_->rotoBrushMask_.release();
        impl_->rotoBrushSource_.release();
        impl_->rotoBrushStrokes_.clear();
        impl_->rotoBrushEngine_->reset();
      }
    }
    layer->changed();
    impl_->publishLayerModified(layer);
    impl_->invalidateBaseComposite();
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    setInfoOverlayText(QStringLiteral("Roto Brush"),
                       QStringLiteral("Generated mask undone"));
    return true;
  }
  auto *paintLayer = layer
                         ? dynamic_cast<ArtifactPaintLayer *>(layer.get())
                         : nullptr;
  if (!paintLayer || !paintLayer->canUndo()) {
    return false;
  }
  paintLayer->undoLastStroke();
  paintLayer->changed();
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  setInfoOverlayText(QStringLiteral("Brush"),
                     QStringLiteral("Last paint stroke undone"));
  return true;
}

bool CompositionRenderController::cancelCloneStamp() {
  if (!impl_ || (!impl_->cloneStampSourceSet_ &&
                 !impl_->cloneStampDragging_)) {
    return false;
  }
  impl_->cloneStampDragging_ = false;
  impl_->cloneStampSourceSet_ = false;
  impl_->cloneStampSourceLayer_.reset();
  impl_->cloneStampSourceCanvas_ = {};
  impl_->cloneStampStartCanvas_ = {};
  impl_->cloneStampLastCanvas_ = {};
  impl_->brushCursorVisible_ = false;
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

} // namespace Artifact
