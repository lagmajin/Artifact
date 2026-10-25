module;

#include <QColor>
#include <QString>

module Artifact.Widgets.TimelineLayerTest;

import Artifact.Layer.Abstract;
import Artifact.Layers.SolidImage;
import Memory.SharedPtr;

namespace Artifact {

bool timelineLayerTestSolidLayerInfo(const ArtifactAbstractLayerPtr& layer,
                                     QColor& fillColor, double& opacity,
                                     QString& layerName)
{
    const auto solidLayer =
        ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer);
    if (!solidLayer) {
        return false;
    }
    const auto color = solidLayer->color();
    fillColor = QColor::fromRgbF(color.r(), color.g(), color.b(), color.a());
    opacity = color.alpha();
    layerName = solidLayer->layerName();
    return true;
}

void setTimelineLayerTestSolidLayerOpacity(
    const ArtifactAbstractLayerPtr& layer, const double opacity)
{
    const auto solidLayer =
        ArtifactCore::dynamicPointerCast<ArtifactSolidImageLayer>(layer);
    if (!solidLayer) {
        return;
    }
    auto color = solidLayer->color();
    color.setAlpha(static_cast<float>(opacity));
    solidLayer->setColor(color);
}

} // namespace Artifact
