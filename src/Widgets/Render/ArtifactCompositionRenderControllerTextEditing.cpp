module;

#define QT_NO_KEYWORDS

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <vector>

#include <QChar>
#include <QInputDialog>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTextDocument>
#include <QTransform>
#include <QtGlobal>

module Artifact.Widgets.CompositionRenderController;

import :Impl;
import :ViewportGeometry;
import Artifact.Application.Manager;
import Artifact.Composition.Abstract;
import Artifact.Event.Types;
import Artifact.Layer.Abstract;
import Artifact.Layer.InitParams;
import Artifact.Layers.Selection.Manager;
import Artifact.Layer.Text;
import Artifact.Preview.Pipeline;
import Artifact.Render.IRenderer;
import Artifact.Service.Project;
import Artifact.Widgets.CompositionTextPuppetUndoCommands;
import Artifact.Widgets.TextGizmo;
import Color.Float;
import Event.Bus;
import Frame.Position;
import Frame.Rate;
import Math.Interpolate;
import Math.Vec;
import Memory.SharedPtr;
import Property.Abstract;
import Text.LayoutContract;
import Time.Rational;
import Undo.UndoManager;
import Utils.Id;
import Utils.String.UniString;

namespace Artifact {

using namespace CompositionRenderControllerDetail;


namespace {

std::vector<int> buildTextCodepointUtf16Offsets(const QString &text) {
  std::vector<int> offsets;
  offsets.reserve(static_cast<size_t>(text.size()) + 1);
  offsets.push_back(0);
  for (int i = 0; i < text.size();) {
    const QChar ch = text.at(i);
    const int unitLength =
        (ch.isHighSurrogate() && i + 1 < text.size() &&
         text.at(i + 1).isLowSurrogate())
            ? 2
            : 1;
    i += unitLength;
    offsets.push_back(i);
  }
  return offsets;
}

int textCodepointCountOf(const QString &text) {
  return static_cast<int>(buildTextCodepointUtf16Offsets(text).size()) - 1;
}

int textCodepointIndexForUtf16Pos(const QString &text, int utf16Pos) {
  int cpIndex = 0;
  int i = 0;
  utf16Pos = std::clamp(utf16Pos, 0, static_cast<int>(text.size()));
  while (i < utf16Pos) {
    const QChar ch = text.at(i);
    i += (ch.isHighSurrogate() && i + 1 < text.size() &&
          text.at(i + 1).isLowSurrogate())
             ? 2
             : 1;
    ++cpIndex;
  }
  return cpIndex;
}

struct TextSessionCaretGeometry {
  bool valid = false;
  bool vertical = false;
  float flowPos = 0.0f;
  float crossStart = 0.0f;
  float crossEnd = 0.0f;
};

TextSessionCaretGeometry textSessionCaretGeometryForCp(
    const std::vector<ArtifactCore::GlyphItem> &glyphs, int cpIndex,
    bool vertical) {
  TextSessionCaretGeometry geometry;
  geometry.vertical = vertical;
  if (glyphs.empty()) {
    return geometry;
  }
  const ArtifactCore::GlyphItem *caretBefore = nullptr;
  const ArtifactCore::GlyphItem *caretAfter = nullptr;
  for (const auto &glyph : glyphs) {
    if (!caretBefore && glyph.index >= cpIndex) {
      caretBefore = &glyph;
    }
    if (glyph.index < cpIndex) {
      caretAfter = &glyph;
    }
  }
  const ArtifactCore::GlyphItem *anchorGlyph = nullptr;
  bool after = false;
  if (caretBefore && caretBefore->index == cpIndex) {
    anchorGlyph = caretBefore;
  } else if (caretAfter) {
    anchorGlyph = caretAfter;
    after = true;
  } else if (caretBefore) {
    anchorGlyph = caretBefore;
  }
  if (!anchorGlyph) {
    return geometry;
  }
  geometry.valid = true;
  const QRectF &bounds = anchorGlyph->bounds;
  if (vertical) {
    geometry.flowPos = static_cast<float>(after ? bounds.bottom()
                                                : bounds.top());
    geometry.crossStart = static_cast<float>(bounds.left());
    geometry.crossEnd = static_cast<float>(bounds.right());
  } else {
    geometry.flowPos = static_cast<float>(after ? bounds.right()
                                                : bounds.left());
    geometry.crossStart = static_cast<float>(bounds.top());
    geometry.crossEnd = static_cast<float>(bounds.bottom());
  }
  return geometry;
}

int textSessionCaretPositionForLocalPoint(
    const std::vector<ArtifactCore::GlyphItem> &glyphs, const QString &text,
    const QPointF &localPoint, bool vertical) {
  if (glyphs.empty()) {
    return text.size();
  }
  const auto cpOffsets = buildTextCodepointUtf16Offsets(text);
  const int cpCount = static_cast<int>(cpOffsets.size()) - 1;
  const auto utf16ForCp = [&](int cp) {
    cp = std::clamp(cp, 0, cpCount);
    return cpOffsets[static_cast<size_t>(cp)];
  };

  std::map<int, QRectF> lineBoxes;
  for (const auto &glyph : glyphs) {
    auto &box = lineBoxes[glyph.lineIndex];
    box = box.isNull() ? glyph.bounds : box.united(glyph.bounds);
  }
  const float lineCoord = static_cast<float>(vertical ? localPoint.x()
                                                      : localPoint.y());
  int targetLine = -1;
  float bestLineDistance = std::numeric_limits<float>::max();
  for (const auto &[lineIndex, box] : lineBoxes) {
    const float lineStart =
        static_cast<float>(vertical ? box.left() : box.top());
    const float lineEnd =
        static_cast<float>(vertical ? box.right() : box.bottom());
    const float distance = lineCoord < lineStart
                               ? lineStart - lineCoord
                               : (lineCoord > lineEnd ? lineCoord - lineEnd
                                                      : 0.0f);
    if (distance < bestLineDistance) {
      bestLineDistance = distance;
      targetLine = lineIndex;
    }
  }

  const float flow = static_cast<float>(vertical ? localPoint.y()
                                                 : localPoint.x());
  bool haveBest = false;
  int bestCp = 0;
  float bestFlowDistance = std::numeric_limits<float>::max();
  for (const auto &glyph : glyphs) {
    if (targetLine >= 0 && glyph.lineIndex != targetLine) {
      continue;
    }
    const float glyphStart =
        static_cast<float>(vertical ? glyph.bounds.top()
                                    : glyph.bounds.left());
    const float glyphEnd = static_cast<float>(vertical
                                                  ? glyph.bounds.bottom()
                                                  : glyph.bounds.right());
    const int startCp = glyph.index;
    const int endCp = startCp + textCodepointCountOf(glyph.clusterText);
    const auto consider = [&](int cp, float pos) {
      const float distance = std::abs(pos - flow);
      if (!haveBest || distance < bestFlowDistance) {
        haveBest = true;
        bestCp = cp;
        bestFlowDistance = distance;
      }
    };
    consider(startCp, glyphStart);
    consider(endCp, glyphEnd);
  }
  return haveBest ? utf16ForCp(bestCp) : text.size();
}

qint64 textSessionFrame(const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer) {
  if (!layer) {
    return 0;
  }
  if (auto *composition = static_cast<ArtifactAbstractComposition *>(
          layer->composition())) {
    return composition->framePosition().framePosition();
  }
  return 0;
}

QString textSessionValue(
    const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer) {
  if (!layer) {
    return {};
  }
  return layer->hasSourceTextKeyframes()
             ? layer->sourceTextAtFrame(textSessionFrame(layer))
             : layer->text().toQString();
}

bool commitTextSessionValue(
    const ArtifactCore::SharedPtr<ArtifactTextLayer> &layer,
    const QString &beforeText, const QString &nextText) {
  if (!layer || beforeText == nextText) {
    return false;
  }
  if (layer->hasSourceTextKeyframes()) {
    const auto property = layer->getProperty(QStringLiteral("text.value"));
    if (!property) {
      return false;
    }
    const auto beforeKeyframes = property->getKeyFrames();
    auto afterKeyframes = beforeKeyframes;
    const qint64 frame = textSessionFrame(layer);
    auto *composition = static_cast<ArtifactAbstractComposition *>(
        layer->composition());
    const double rate = composition ? composition->frameRate().framerate()
                                    : 30.0;
    const int fps = std::max(1, static_cast<int>(std::llround(
                                    std::isfinite(rate) && rate > 0.0
                                        ? rate
                                        : 30.0)));
    ArtifactCore::KeyFrame editedKeyframe;
    editedKeyframe.time = RationalTime(frame, fps);
    editedKeyframe.value = nextText;
    editedKeyframe.interpolation = ArtifactCore::InterpolationType::Constant;
    const auto existing = std::find_if(
        afterKeyframes.begin(), afterKeyframes.end(),
        [&editedKeyframe](const ArtifactCore::KeyFrame &keyframe) {
          return keyframe.time == editedKeyframe.time;
        });
    if (existing != afterKeyframes.end()) {
      *existing = editedKeyframe;
    } else {
      afterKeyframes.push_back(editedKeyframe);
    }
    if (auto *manager = UndoManager::instance()) {
      return manager->push(std::make_unique<SetLayerPropertyKeyframesCommand>(
          layer, QStringLiteral("text.value"), beforeKeyframes,
          afterKeyframes, QStringLiteral("Edit Source Text")));
    }
    layer->setSourceTextAtFrame(frame, nextText);
    return layer->sourceTextAtFrame(frame) == nextText;
  }
  if (auto *manager = UndoManager::instance()) {
    return manager->push(std::make_unique<SetTextLayerTextCommand>(
        layer, beforeText, nextText, QStringLiteral("Edit Text")));
  }
  return layer->setLayerPropertyValue(QStringLiteral("text.value"),
                                      nextText);
}

} // namespace

void CompositionRenderController::Impl::drawTextSessionOverlay(
    ArtifactIRenderer &renderer,
    const ArtifactCore::SharedPtr<ArtifactTextLayer> &textLayer,
    const QString &editText, int cursorPos, int selectionAnchor,
    const QString &preeditText, bool caretVisible) {
  const auto glyphGeometry = textLayer->textGlyphGeometry();
  if (glyphGeometry.glyphs.empty()) {
    return;
  }
  const QTransform transform = textLayer->getGlobalTransform();
  const QPointF &origin = glyphGeometry.drawOrigin;
  const bool vertical =
      textLayer->writingMode() == TextWritingMode::Vertical;
  const float zoom = renderer.getZoom();
  const float invZoom =
      std::isfinite(zoom) && zoom > 0.0001f ? 1.0f / zoom : 1.0f;

  const auto drawGlyphRangeQuads = [&](int utf16Start, int utf16End,
                                       const FloatColor &color,
                                       float heightFraction) {
    if (utf16End <= utf16Start) {
      return;
    }
    const int cpStart = textCodepointIndexForUtf16Pos(editText, utf16Start);
    const int cpEnd = textCodepointIndexForUtf16Pos(editText, utf16End);
    std::map<int, QRectF> ranges;
    for (const auto &glyph : glyphGeometry.glyphs) {
      const int glyphEndCp =
          glyph.index + textCodepointCountOf(glyph.clusterText);
      if (glyphEndCp <= cpStart || glyph.index >= cpEnd) {
        continue;
      }
      auto &box = ranges[glyph.lineIndex];
      box = box.isNull() ? glyph.bounds : box.united(glyph.bounds);
    }
    for (const auto &[lineIndex, box] : ranges) {
      QRectF quad = box;
      if (heightFraction < 1.0f) {
        const qreal keepHeight = quad.height() * heightFraction;
        quad.setTop(quad.bottom() - keepHeight);
      }
      renderer.drawSolidRectTransformed(
          static_cast<float>(quad.left() + origin.x()),
          static_cast<float>(quad.top() + origin.y()),
          static_cast<float>(quad.width()),
          static_cast<float>(quad.height()), transform, color);
    }
  };

  const int selectionStart = std::min(selectionAnchor, cursorPos);
  const int selectionEnd = std::max(selectionAnchor, cursorPos);
  if (selectionEnd > selectionStart) {
    drawGlyphRangeQuads(selectionStart, selectionEnd,
                        FloatColor{0.30f, 0.55f, 1.0f, 0.32f}, 1.0f);
  }
  if (!preeditText.isEmpty()) {
    drawGlyphRangeQuads(std::max(0, cursorPos - static_cast<int>(preeditText.size())),
                        cursorPos, FloatColor{1.0f, 0.85f, 0.30f, 0.45f},
                        0.22f);
  }
  if (!caretVisible) {
    return;
  }
  const int caretCp = textCodepointIndexForUtf16Pos(editText, cursorPos);
  const auto caret = textSessionCaretGeometryForCp(
      glyphGeometry.glyphs, caretCp, vertical);
  if (!caret.valid) {
    return;
  }
  const float caretThickness = std::max(1.2f, 1.6f * invZoom);
  if (!caret.vertical) {
    const QPointF topPoint = transform.map(
        QPointF(caret.flowPos + origin.x(), caret.crossStart + origin.y()));
    const QPointF bottomPoint = transform.map(
        QPointF(caret.flowPos + origin.x(), caret.crossEnd + origin.y()));
    renderer.drawSolidLine(
        {static_cast<float>(topPoint.x()) + 0.8f,
         static_cast<float>(topPoint.y()) + 0.8f},
        {static_cast<float>(bottomPoint.x()) + 0.8f,
         static_cast<float>(bottomPoint.y()) + 0.8f},
        FloatColor{0.0f, 0.0f, 0.0f, 0.60f}, caretThickness);
    renderer.drawSolidLine(
        {static_cast<float>(topPoint.x()),
         static_cast<float>(topPoint.y())},
        {static_cast<float>(bottomPoint.x()),
         static_cast<float>(bottomPoint.y())},
        FloatColor{0.95f, 0.98f, 1.0f, 1.0f}, caretThickness);
  } else {
    const QPointF leftPoint = transform.map(
        QPointF(caret.crossStart + origin.x(), caret.flowPos + origin.y()));
    const QPointF rightPoint = transform.map(
        QPointF(caret.crossEnd + origin.x(), caret.flowPos + origin.y()));
    renderer.drawSolidLine(
        {static_cast<float>(leftPoint.x()),
         static_cast<float>(leftPoint.y())},
        {static_cast<float>(rightPoint.x()),
         static_cast<float>(rightPoint.y())},
        FloatColor{0.95f, 0.98f, 1.0f, 1.0f}, caretThickness);
  }
}

bool CompositionRenderController::startTextEditSession(
    const ArtifactAbstractLayerPtr &layer) {
  if (!impl_ || !layer) {
    return false;
  }
  const auto textLayer =
      ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer);
  if (!textLayer) {
    return false;
  }
  if (impl_->textEditSessionActive_) {
    finishTextEditSession(false);
  }
  const QString sessionText = textSessionValue(textLayer);
  if (Qt::mightBeRichText(sessionText) ||
      textLayer->layoutMode() == TextLayoutMode::Path ||
      textLayer->textGlyphGeometry().glyphs.empty()) {
    return false;
  }
  impl_->textEditSessionActive_ = true;
  impl_->textEditSessionLayerId_ = layer->id();
  impl_->textEditSessionStartText_ = sessionText;
  impl_->textEditSessionEditText_ = sessionText;
  impl_->textEditCursorPos_ = sessionText.length();
  impl_->textEditSelectionAnchor_ = sessionText.length();
  impl_->textEditPreeditText_.clear();
  impl_->textEditCaretVisible_ = true;
  if (impl_->textGizmo_) {
    impl_->textGizmo_->setEditSessionActive(true);
  }
  setInfoOverlayText(
      QStringLiteral("Text Edit"),
      QStringLiteral("Type to edit — Ctrl+Enter commit, Esc cancel"));
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

void CompositionRenderController::finishTextEditSession(
    bool discardChanges) {
  if (!impl_ || !impl_->textEditSessionActive_) {
    return;
  }
  auto comp = impl_->previewPipeline_.composition();
  const auto layer = comp ? comp->layerById(impl_->textEditSessionLayerId_)
                          : ArtifactAbstractLayerPtr{};
  const auto textLayer =
      ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(layer);
  const QString startText = impl_->textEditSessionStartText_;
  const QString editText = impl_->textEditSessionEditText_;
  impl_->textEditSessionActive_ = false;
  impl_->textEditSessionLayerId_ = LayerID{};
  impl_->textEditCursorPos_ = 0;
  impl_->textEditSelectionAnchor_ = 0;
  impl_->textEditPreeditText_.clear();
  impl_->textEditCaretVisible_ = true;
  if (impl_->textGizmo_) {
    impl_->textGizmo_->setEditSessionActive(false);
  }
  if (!textLayer) {
    impl_->invalidateOverlayComposite();
    markRenderDirty();
    return;
  }
  if (discardChanges) {
    textLayer->setText(UniString(startText));
    textLayer->setDirty();
    textLayer->updateImage();
    textLayer->changed();
  } else if (commitTextSessionValue(textLayer, startText, editText)) {
    if (auto *composition = static_cast<ArtifactAbstractComposition *>(
            textLayer->composition())) {
      ArtifactCore::globalEventBus().publish<LayerChangedEvent>(
          LayerChangedEvent{composition->id().toString(),
                            textLayer->id().toString(),
                            LayerChangedEvent::ChangeType::Modified});
    }
  }
  setInfoOverlayText(QString(), QString());
  impl_->invalidateOverlayComposite();
  markRenderDirty();
}

bool CompositionRenderController::isTextEditSessionActive() const {
  return impl_ && impl_->textEditSessionActive_;
}

ArtifactAbstractLayerPtr CompositionRenderController::textEditSessionLayer()
    const {
  if (!impl_ || !impl_->textEditSessionActive_) {
    return {};
  }
  auto comp = impl_->previewPipeline_.composition();
  return comp ? comp->layerById(impl_->textEditSessionLayerId_)
              : ArtifactAbstractLayerPtr{};
}

QString CompositionRenderController::textEditSessionText() const {
  return impl_ ? impl_->textEditSessionEditText_ : QString();
}

void CompositionRenderController::textSessionSyncFromEditor(
    const QString &text, int cursorPos, int selectionAnchor,
    const QString &preedit) {
  if (!impl_ || !impl_->textEditSessionActive_) {
    return;
  }
  impl_->textEditSessionEditText_ = text;
  impl_->textEditCursorPos_ = cursorPos;
  impl_->textEditSelectionAnchor_ = selectionAnchor;
  impl_->textEditPreeditText_ = preedit;
  impl_->textEditCaretVisible_ = true;
  auto comp = impl_->previewPipeline_.composition();
  const auto textLayer = comp
      ? ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
            comp->layerById(impl_->textEditSessionLayerId_))
      : nullptr;
  if (textLayer) {
    textLayer->setText(UniString(text));
    textLayer->setDirty();
    textLayer->updateImage();
    textLayer->changed();
  }
  markRenderDirty();
}

void CompositionRenderController::textSessionToggleCaretBlink() {
  if (!impl_ || !impl_->textEditSessionActive_) {
    return;
  }
  impl_->textEditCaretVisible_ = !impl_->textEditCaretVisible_;
  markRenderDirty();
}

int CompositionRenderController::textSessionCaretPositionAt(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) const {
  if (!impl_ || !impl_->textEditSessionActive_ || !impl_->renderer_) {
    return -1;
  }
  auto comp = impl_->previewPipeline_.composition();
  const auto textLayer = comp
      ? ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(
            comp->layerById(impl_->textEditSessionLayerId_))
      : nullptr;
  if (!textLayer) {
    return -1;
  }
  const auto glyphGeometry = textLayer->textGlyphGeometry();
  if (glyphGeometry.glyphs.empty()) {
    return -1;
  }
  const QPointF physicalPos =
      ArtifactCore::Coordinates::toQPointF(viewportPos) *
      impl_->devicePixelRatio_;
  const auto canvasPoint = impl_->renderer_->viewportToCanvas(
      {static_cast<float>(physicalPos.x()),
       static_cast<float>(physicalPos.y())});
  bool invertible = false;
  const QTransform inverse =
      textLayer->getGlobalTransform().inverted(&invertible);
  if (!invertible) {
    return -1;
  }
  const QPointF localPoint =
      inverse.map(QPointF(canvasPoint.x, canvasPoint.y)) -
      glyphGeometry.drawOrigin;
  return textSessionCaretPositionForLocalPoint(
      glyphGeometry.glyphs, impl_->textEditSessionEditText_, localPoint,
      textLayer->writingMode() == TextWritingMode::Vertical);
}

bool CompositionRenderController::createTextLayerAtCanvas(
    ArtifactCore::Coordinates::CompositionPoint2 canvasPos,
    const QSizeF& boxSize) {
  const auto comp = impl_ ? impl_->previewPipeline_.composition()
                          : ArtifactCompositionPtr{};
  auto *service = ArtifactProjectService::instance();
  if (!impl_ || !comp || !service) {
    return false;
  }
  if (!std::isfinite(canvasPos.x) || !std::isfinite(canvasPos.y)) {
    return false;
  }
  const bool finiteBox = std::isfinite(boxSize.width()) &&
                         std::isfinite(boxSize.height());
  if (!finiteBox) {
    return false;
  }
  const QString layerName = uniqueLayerNameForCurrentComposition(
      QStringLiteral("Text"));
  const LayerID previousSelectedLayerId = impl_->selectedLayerId_;
  ArtifactTextLayerInitParams params(layerName);
  service->addLayerToCurrentComposition(params, true);
  auto *selectionManager = ArtifactApplicationManager::instance()
                               ? ArtifactApplicationManager::instance()
                                     ->layerSelectionManager()
                               : nullptr;
  auto created = selectionManager ? selectionManager->currentLayer()
                                  : ArtifactAbstractLayerPtr{};
  auto textLayer = created
                       ? ArtifactCore::dynamicPointerCast<ArtifactTextLayer>(created)
                       : ArtifactCore::SharedPtr<ArtifactTextLayer>{};
  if (!textLayer || created->id() == previousSelectedLayerId) {
    return false;
  }
  textLayer->setText(UniString(QStringLiteral("Text")));
  if (finiteBox && boxSize.width() > 1.0 && boxSize.height() > 1.0) {
    textLayer->setLayoutMode(TextLayoutMode::Box);
    textLayer->setMaxWidth(static_cast<float>(boxSize.width()));
    textLayer->setBoxHeight(static_cast<float>(boxSize.height()));
  } else {
    textLayer->setLayoutMode(TextLayoutMode::Point);
  }
  textLayer->setPosition3D({canvasPos.x, canvasPos.y,
                            textLayer->position3D().z});
  textLayer->changed();
  impl_->selectedLayerId_ = created->id();
  impl_->previewPipeline_.setSelectedLayerId(created->id());
  impl_->sync2DGizmosForLayer(created);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

bool CompositionRenderController::editTextAtViewport(
    ArtifactCore::Coordinates::ScreenLogicalPoint2 viewportPos) {
  if (!impl_) return false;
  const auto composition = impl_->previewPipeline_.composition();
  auto layer = composition
      ? composition->layerById(impl_->selectedLayerId_)
      : ArtifactAbstractLayerPtr{};
  auto *textLayer = layer ? dynamic_cast<ArtifactTextLayer *>(layer.get()) : nullptr;
  bool selectedTextHit = false;
  if (textLayer && composition && impl_->renderer_) {
    const auto viewportPhysical =
        ArtifactCore::Coordinates::toScreenPhysical(
            viewportPos, impl_->devicePixelRatio_);
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {viewportPhysical.x, viewportPhysical.y});
    bool invertible = false;
    const QTransform inverse = layer->getGlobalTransform().inverted(&invertible);
    selectedTextHit = !layer->isLocked() && invertible &&
        layer->localBounds().contains(inverse.map(QPointF(canvas.x, canvas.y)));
    if (!selectedTextHit) {
      layer = ArtifactAbstractLayerPtr{};
      textLayer = nullptr;
    }
  }
  if (!textLayer && composition && impl_->renderer_) {
    const auto viewportPhysical =
        ArtifactCore::Coordinates::toScreenPhysical(
            viewportPos, impl_->devicePixelRatio_);
    const auto canvas = impl_->renderer_->viewportToCanvas(
        {viewportPhysical.x, viewportPhysical.y});
    const auto currentFrame = currentFrameForComposition(composition);
    const auto &layers = composition->allLayerRef();
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
      const auto &candidate = layers[static_cast<size_t>(i)];
      auto *candidateText = dynamic_cast<ArtifactTextLayer *>(candidate.get());
      if (!candidateText || candidate->isLocked() ||
          candidate->isSelectionLocked() ||
          !isLayerEffectivelyVisible(candidate) ||
          !candidate->isActiveAt(currentFrame)) {
        continue;
      }
      bool invertible = false;
      const QTransform inverse = candidate->getGlobalTransform().inverted(&invertible);
      if (invertible && candidate->localBounds().contains(
                            inverse.map(QPointF(canvas.x, canvas.y)))) {
        layer = candidate;
        textLayer = candidateText;
        break;
      }
    }
  }
  if (!textLayer) return false;
  if (layer->id() != impl_->selectedLayerId_) {
    setSelectedLayerId(layer->id());
  }

  bool accepted = false;
  const QString edited = QInputDialog::getMultiLineText(
      nullptr, QStringLiteral("Edit Text"), QStringLiteral("Text:"),
      textLayer->text().toQString(), &accepted);
  if (!accepted || edited == textLayer->text().toQString()) return accepted;

  const QString before = textLayer->text().toQString();
  auto *manager = UndoManager::instance();
  if (manager) {
    if (!manager->push(std::make_unique<TextContentUndoCommand>(
            layer, before, edited))) {
      return false;
    }
  } else {
    textLayer->setText(UniString(edited));
  }
  impl_->publishLayerModified(layer);
  impl_->invalidateBaseComposite();
  impl_->invalidateOverlayComposite();
  markRenderDirty();
  return true;
}

} // namespace Artifact
