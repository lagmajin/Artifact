module;
#include <utility>
#include <QObject>

module Artifact.Layer.AdjustableLayer;

import Size;
import Artifact.Layers.Abstract._2D;
import Artifact.Composition.Abstract;

namespace Artifact
{

ArtifactAdjustableLayer::ArtifactAdjustableLayer()
{
    setLayerName("Adjustment Layer");
    setAdjustmentLayer(true);
}

ArtifactAdjustableLayer::~ArtifactAdjustableLayer() = default;

void ArtifactAdjustableLayer::setComposition(QObject* comp)
{
    setComposition(static_cast<void*>(comp));
}

void ArtifactAdjustableLayer::setComposition(void *comp)
{
    ArtifactAbstractLayer::setComposition(comp);
    if (auto *composition = static_cast<ArtifactAbstractComposition*>(comp)) {
        const auto compSize = composition->settings().compositionSize();
        if (compSize.width() > 0 && compSize.height() > 0) {
            setSourceSize(Size_2D{compSize.width(), compSize.height()});
        }
    }
}

void ArtifactAdjustableLayer::draw(ArtifactIRenderer* renderer)
{
    (void)renderer;
    // Adjustment layers are effect carriers. They do not draw their own pixels.
    // Their effects are applied by the composition renderer when appropriate.
}

bool ArtifactAdjustableLayer::isAdjustmentLayer() const
{
    return true;
}

QJsonObject ArtifactAdjustableLayer::toJson() const
{
    QJsonObject obj = ArtifactAbstract2DLayer::toJson();
    obj[QStringLiteral("type")] = static_cast<int>(LayerType::Adjustment);
    return obj;
}

void ArtifactAdjustableLayer::fromJsonProperties(const QJsonObject& obj)
{
    ArtifactAbstract2DLayer::fromJsonProperties(obj);
}

bool ArtifactAdjustableLayer::isNullLayer() const
{
    return false;
}

}
