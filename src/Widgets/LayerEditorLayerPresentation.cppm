module;

#include <QWidget>

module Artifact.Widgets.LayerEditor.LayerPresentation;

import Artifact.Layer.Image;
import Artifact.Layer.Shape;
import Memory.SharedPtr;

namespace Artifact {

QString layerEditorLayerTypeLabel(const ArtifactAbstractLayerPtr& layer)
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
  QString spaced;
  spaced.reserve(type.size() + 4);
  for (qsizetype index = 0; index < type.size(); ++index) {
   if (index > 0 && type.at(index).isUpper() &&
       type.at(index - 1).isLower()) {
    spaced.append(QStringLiteral(" "));
   }
   spaced.append(type.at(index));
  }
  type = spaced;
 }

 const QString dimension = layer->is3D()
     ? QStringLiteral("3D") : QStringLiteral("2D");
 if (!type.contains(dimension, Qt::CaseInsensitive)) {
  type += QStringLiteral(" %1").arg(dimension);
 }
 return type;
}

QString layerEditorLayerNameLabel(const ArtifactAbstractLayerPtr& layer)
{
 if (!layer) return QStringLiteral("No layer selected");
 const QString name = layer->layerName().trimmed();
 return name.isEmpty() ? QStringLiteral("Untitled Layer") : name;
}

void publishLayerEditorReadout(
    QWidget* widget,
    const ArtifactAbstractLayerPtr& layer,
    bool isActive)
{
 if (!widget) return;
 const QString name = layerEditorLayerNameLabel(layer);
 widget->setProperty("artifactLayerName", name);
 widget->setProperty("artifactLayerType", layerEditorLayerTypeLabel(layer));
 widget->setProperty("artifactLayerVisible", layer && layer->isVisible());
 widget->setProperty("artifactLayerLocked", layer && layer->isLocked());
 widget->setProperty("artifactLayerSolo", layer && layer->isSolo());
 widget->setProperty("artifactLayerActive", isActive);
 widget->setProperty(
     "artifactLayerCacheState",
     !layer || !layer->usesLayerCache()
         ? QStringLiteral("Off")
         : layer->isDirty() ? QStringLiteral("Dirty")
                            : QStringLiteral("Ready"));
 widget->setAccessibleName(layer
     ? QStringLiteral("Layer Solo View — %1").arg(name)
     : QStringLiteral("Layer Solo View"));
}

}
