module;

#include <QString>

module Artifact.Project;

import Artifact.Layer.Abstract;
import Property.Abstract;
import Property.Group;
import Property.Types;
import Property.SerializationBridge;
import Memory.SharedPtr;

namespace Artifact {

void copyArtifactLayerProperties(
    const ArtifactAbstractLayerPtr& sourceLayer,
    const ArtifactAbstractLayerPtr& duplicatedLayer,
    const QString& duplicatedName)
{
    if (!sourceLayer || !duplicatedLayer) {
        return;
    }
    for (const auto& group : sourceLayer->getLayerPropertyGroups()) {
        for (const auto& property : group.allProperties()) {
            if (!property) {
                continue;
            }
            const QString propertyName = property->getName();
            if (propertyName == QStringLiteral("layer.name")) {
                continue;
            }
            auto duplicatedProperty = duplicatedLayer->getProperty(propertyName);
            if (!duplicatedProperty) {
                QVariant value = property->getValue();
                if (property->getType() == ArtifactCore::PropertyType::Color) {
                    value = property->getColorValue();
                }
                duplicatedLayer->setLayerPropertyValue(propertyName, value);
                continue;
            }
            const auto serialized =
                ArtifactCore::PropertySerializationBridge::serializeProperty(property);
            duplicatedProperty->clearKeyFrames();
            duplicatedProperty->clearEnvelopes();
            ArtifactCore::PropertySerializationBridge::deserializeProperty(
                duplicatedProperty, serialized);
            if (propertyName == QStringLiteral("mpmSand.gravityY")) {
                duplicatedProperty->setAnimatable(
                    property->isAnimatable() || !serialized.keyframes.isEmpty());
                duplicatedLayer->setLayerPropertyValue(
                    propertyName, duplicatedProperty->getValue());
            }
        }
    }
    duplicatedLayer->setLayerName(duplicatedName);
    duplicatedLayer->setBlendMode(sourceLayer->layerBlendType());
    if (sourceLayer->hasParent()) {
        duplicatedLayer->setParentById(sourceLayer->parentLayerId());
    } else {
        duplicatedLayer->clearParent();
    }
}

} // namespace Artifact
