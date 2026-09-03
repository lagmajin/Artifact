module;

#include <QRectF>
#include <QString>

#include <algorithm>

module Artifact.Widgets.LayerEditor.SurfaceInfo;

import Artifact.Composition.Abstract;
import Artifact.Effect.Abstract;
import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Artifact.Mask.LayerMask;
import Artifact.Mask.Path;
import Artifact.Widgets.LayerEditor.ViewportChrome;
import Frame.Position;
import Layer.Blend;
import Memory.SharedPtr;
import Tool;

namespace Artifact {
namespace {

QString surfaceLayerName(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) return QStringLiteral("No layer selected");
 const QString name = layer->layerName().trimmed();
 return name.isEmpty() ? QStringLiteral("Untitled Layer") : name;
}

QString surfaceLayerType(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) return QStringLiteral("—");
 QString type;
 if (ArtifactCore::dynamicPointerCast<ArtifactShapeLayer>(
         ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer))) {
  type = QStringLiteral("Shape");
 } else if (ArtifactCore::dynamicPointerCast<ArtifactImageLayer>(
                ArtifactCore::SharedPtr<ArtifactAbstractLayer>(layer))) {
  type = QStringLiteral("Image");
 } else {
  type = layer->className().toQString().trimmed();
  if (type.startsWith(QStringLiteral("Artifact"))) type.remove(0, 8);
  if (type.endsWith(QStringLiteral("Layer"))) type.chop(5);
  if (type.isEmpty()) type = QStringLiteral("Layer");
 }
 const QString dimension = layer->is3D() ? QStringLiteral("3D") : QStringLiteral("2D");
 if (!type.contains(dimension, Qt::CaseInsensitive)) type += QStringLiteral(" %1").arg(dimension);
 return type;
}

}

LayerEditorSurfaceInfo buildLayerEditorSurfaceInfo(
    const ArtifactAbstractLayerPtr& layer,
    const ArtifactCompositionPtr& composition,
    const FramePosition& currentFrame, DisplayMode displayMode,
    bool inspectMode)
{
 LayerEditorSurfaceInfo result;
 result.title = inspectMode
     ? QStringLiteral("Inspect") : QStringLiteral("Impact");
 result.body = QStringLiteral("No layer selected");
 result.parentLayerIds.clear();
 result.childLayerIds.clear();
 result.matteLayerIds.clear();
 result.dependentLayerIds.clear();
 if (!layer) return result;

 result.title += QStringLiteral(" · %1").arg(surfaceLayerName(layer));
 if (inspectMode) {
  result.title += QStringLiteral(" · Frame %1")
      .arg(currentFrame.framePosition());
  const auto source = layer->sourceSize();
  const QRectF bounds = layer->transformedBoundingBox();
  const auto& transform = layer->transform3D();
  const auto layerId = layer->id();
  int enabledMatteCount = 0;
  for (const auto& ref : layer->matteReferences()) {
   if (ref.enabled && !ref.sourceLayerId.isNil() &&
       ref.sourceLayerId != layerId) {
    ++enabledMatteCount;
   }
  }
  QString effectSummary = QStringLiteral("None");
  const auto effects = layer->getEffects();
  if (!effects.empty()) {
   effectSummary.clear();
   constexpr int maxNamedEffects = 2;
   const int namedEffectCount = std::min(
       static_cast<int>(effects.size()), maxNamedEffects);
   for (int i = 0; i < namedEffectCount; ++i) {
    if (i > 0) effectSummary += QStringLiteral(" › ");
    const auto& effect = effects[static_cast<size_t>(i)];
    const QString name = effect
        ? effect->displayName().toQString().trimmed() : QString{};
    effectSummary += name.isEmpty() ? QStringLiteral("Unnamed")
        : name.size() > 18 ? name.left(17) + QStringLiteral("…") : name;
   }
   if (effects.size() > static_cast<size_t>(namedEffectCount)) {
    const int remainingEffectCount = static_cast<int>(effects.size()) -
        namedEffectCount;
    effectSummary += QStringLiteral("  +%1")
        .arg(remainingEffectCount);
   }
  }
  QString maskSummary = QStringLiteral("None");
  if (layer->maskCount() > 0) {
   const LayerMask firstMask = layer->mask(0);
   QString pathName = QStringLiteral("Mask 1");
   QString modeName = QStringLiteral("Empty");
   QString opacityText = QStringLiteral("—");
   QString invertedText;
   if (firstMask.maskPathCount() > 0) {
    const MaskPath path = firstMask.maskPath(0);
    const QString candidateName = path.name().toQString().trimmed();
    if (!candidateName.isEmpty()) {
     pathName = candidateName.size() > 16
         ? candidateName.left(15) + QStringLiteral("…")
         : candidateName;
    }
    switch (path.mode()) {
    case MaskMode::Subtract: modeName = QStringLiteral("Subtract"); break;
    case MaskMode::Intersect: modeName = QStringLiteral("Intersect"); break;
    case MaskMode::Difference: modeName = QStringLiteral("Difference"); break;
    case MaskMode::Add:
    default: modeName = QStringLiteral("Add"); break;
    }
    opacityText = QStringLiteral("%1%")
        .arg(std::clamp(path.opacity() * 100.0f, 0.0f, 100.0f), 0, 'f', 0);
    invertedText = path.isInverted() ? QStringLiteral(" · Inverted")
                                     : QString{};
   }
   const QString remainingMasks = layer->maskCount() > 1
       ? QStringLiteral(" · +%1").arg(layer->maskCount() - 1)
       : QString{};
   maskSummary = QStringLiteral("%1 · %2 · %3%4%5%6")
       .arg(pathName)
       .arg(modeName)
       .arg(opacityText)
       .arg(invertedText)
       .arg(firstMask.isEnabled() ? QString{}
                                  : QStringLiteral(" · Disabled"))
       .arg(remainingMasks);
  }
  const QString stateText = QStringLiteral("%1 · %2 · %3 · %4")
      .arg(layer->isVisible() ? QStringLiteral("Visible")
                              : QStringLiteral("Hidden"),
           layer->isLocked() ? QStringLiteral("Locked")
                             : QStringLiteral("Unlocked"),
           layer->isSolo() ? QStringLiteral("Solo")
                           : QStringLiteral("Solo off"),
           layer->isActiveAt(currentFrame) ? QStringLiteral("Active")
                                           : QStringLiteral("Out of range"));
  const QString cacheText = !layer->usesLayerCache()
      ? QStringLiteral("Off")
      : layer->isDirty() ? QStringLiteral("Dirty")
                         : QStringLiteral("Ready");
  result.body = QStringLiteral(
      "%1  ·  Stage %2  ·  Source %3 × %4\n"
      "Bounds X %5 Y %6 W %7 H %8  ·  Pivot %9, %10\n"
      "%11\n"
      "Opacity %12%  ·  %13  ·  Matte %14  ·  Cache %15\n"
      "Mask %16\n"
      "FX %17: %18")
      .arg(surfaceLayerType(layer))
      .arg(layerEditorDisplayModeLabel(displayMode))
      .arg(source.width)
      .arg(source.height)
      .arg(bounds.x(), 0, 'f', 0)
      .arg(bounds.y(), 0, 'f', 0)
      .arg(bounds.width(), 0, 'f', 0)
      .arg(bounds.height(), 0, 'f', 0)
      .arg(transform.anchorX(), 0, 'f', 0)
      .arg(transform.anchorY(), 0, 'f', 0)
      .arg(stateText)
      .arg(std::clamp(layer->opacity() * 100.0f, 0.0f, 100.0f), 0, 'f', 0)
      .arg(ArtifactCore::BlendModeUtils::toString(
          ArtifactCore::toBlendMode(layer->layerBlendType())))
      .arg(enabledMatteCount)
      .arg(cacheText)
      .arg(maskSummary)
      .arg(layer->effectCount())
      .arg(effectSummary);
  return result;
 }

 int childCount = 0;
 int dependentCount = 0;
 int matteInputCount = 0;
 QString parentName = QStringLiteral("None");
 QString childNames;
 QString matteNames;
 QString dependentNames;
 const auto appendRelationshipName = [](QString& summary,
                                        const ArtifactAbstractLayerPtr& item,
                                        int visibleIndex) {
  if (!item || visibleIndex >= 1) return;
  QString name = surfaceLayerName(item);
  if (name.size() > 16) name = name.left(15) + QStringLiteral("…");
  if (!summary.isEmpty()) summary += QStringLiteral(", ");
  summary += name;
 };
 if (const auto parent = layer->parentLayer()) {
  parentName = surfaceLayerName(parent);
  if (parentName.size() > 16) {
   parentName = parentName.left(15) + QStringLiteral("…");
  }
  result.parentLayerIds.push_back(parent->id());
 }
 for (const auto& ref : layer->matteReferences()) {
  if (ref.enabled && !ref.sourceLayerId.isNil() &&
      ref.sourceLayerId != layer->id()) {
   ++matteInputCount;
   result.matteLayerIds.push_back(ref.sourceLayerId);
  }
 }
 if (composition) {
   const auto children = composition->childLayersOf(layer->id());
   childCount = static_cast<int>(children.size());
   int namedChildCount = 0;
   for (const auto& child : children) {
    if (child) {
     result.childLayerIds.push_back(child->id());
     appendRelationshipName(childNames, child, namedChildCount++);
    }
   }
   int namedMatteCount = 0;
   for (const auto& matteId : result.matteLayerIds) {
    appendRelationshipName(
        matteNames, composition->layerById(matteId), namedMatteCount++);
   }
   int namedDependentCount = 0;
   for (const auto& candidate : composition->allLayerRef()) {
    if (!candidate || candidate->id() == layer->id()) continue;
    for (const auto& ref : candidate->matteReferences()) {
     if (ref.enabled && ref.sourceLayerId == layer->id()) {
      ++dependentCount;
      result.dependentLayerIds.push_back(candidate->id());
      appendRelationshipName(
          dependentNames, candidate, namedDependentCount++);
      break;
     }
    }
   }
 }
 const auto relationshipSummary = [](int count, const QString& names) {
  if (count <= 0 || names.isEmpty()) return QString::number(count);
  const QString overflow = count > 1
      ? QStringLiteral(", +%1").arg(count - 1) : QString{};
  return QStringLiteral("%1 (%2%3)").arg(count).arg(names, overflow);
 };
 result.body = QStringLiteral(
      "Parent %1  ·  Children %2\n"
      "Matte inputs %3  ·  Used by %4\n"
      "Effects %5  ·  Modifiers %6  ·  Masks %7")
      .arg(parentName)
      .arg(relationshipSummary(childCount, childNames))
      .arg(relationshipSummary(matteInputCount, matteNames))
      .arg(relationshipSummary(dependentCount, dependentNames))
      .arg(layer->effectCount())
      .arg(layer->modifierCount())
      .arg(layer->maskCount());
 return result;
}

}
