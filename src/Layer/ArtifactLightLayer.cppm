module;
#include <QVariant>
#include <wobjectimpl.h>

module Artifact.Layer.Light;

import Artifact.Layer.Abstract;
import Property.Group;
import Property;
import Color.Float;

namespace Artifact {

W_OBJECT_IMPL(ArtifactLightLayer)

struct ArtifactLightLayer::Impl {
    LightType type_ = LightType::Point;
    ArtifactCore::FloatColor color_{1.0f, 1.0f, 1.0f, 1.0f};
    float intensity_ = 100.0f;
    float shadowRadius_ = 10.0f;
    bool castsShadows_ = true;
};

ArtifactLightLayer::ArtifactLightLayer()
    : lightImpl_(new Impl())
{
    setLayerName("Light 1");
}

ArtifactLightLayer::~ArtifactLightLayer()
{
    delete lightImpl_;
}

void ArtifactLightLayer::draw(ArtifactIRenderer* /*renderer*/)
{
    // Light is invisible in final render
}

LightType ArtifactLightLayer::lightType() const { return lightImpl_->type_; }
void ArtifactLightLayer::setLightType(LightType t) { lightImpl_->type_ = t; changed(); }

ArtifactCore::FloatColor ArtifactLightLayer::color() const { return lightImpl_->color_; }
void ArtifactLightLayer::setColor(const ArtifactCore::FloatColor& c) { lightImpl_->color_ = c; changed(); }

float ArtifactLightLayer::intensity() const { return lightImpl_->intensity_; }
void ArtifactLightLayer::setIntensity(float i) { lightImpl_->intensity_ = i; changed(); }

float ArtifactLightLayer::shadowRadius() const { return lightImpl_->shadowRadius_; }
void ArtifactLightLayer::setShadowRadius(float r) { lightImpl_->shadowRadius_ = r; changed(); }

bool ArtifactLightLayer::castsShadows() const { return lightImpl_->castsShadows_; }
void ArtifactLightLayer::setCastsShadows(bool e) { lightImpl_->castsShadows_ = e; changed(); }

std::vector<ArtifactCore::PropertyGroup> ArtifactLightLayer::getLayerPropertyGroups() const
{
    auto groups = ArtifactAbstractLayer::getLayerPropertyGroups();
    
    ArtifactCore::PropertyGroup lightOptions("Light Options");
    // Simplified enum handling for now (could use a specialized enum property)
    lightOptions.addProperty(ArtifactCore::Property("Intensity", lightImpl_->intensity_, 0.0f, 1000.0f, "%"));
    lightOptions.addProperty(ArtifactCore::Property("Shadows", lightImpl_->castsShadows_));
    lightOptions.addProperty(ArtifactCore::Property("Shadow Radius", lightImpl_->shadowRadius_, 0.0f, 500.0f, "px"));
    
    groups.push_back(lightOptions);
    return groups;
}

bool ArtifactLightLayer::setLayerPropertyValue(const QString& propertyPath, const QVariant& value)
{
    if (propertyPath == "Light Options/Intensity") {
        setIntensity(value.toFloat());
        return true;
    } else if (propertyPath == "Light Options/Shadows") {
        setCastsShadows(value.toBool());
        return true;
    } else if (propertyPath == "Light Options/Shadow Radius") {
        setShadowRadius(value.toFloat());
        return true;
    }
    
    return ArtifactAbstractLayer::setLayerPropertyValue(propertyPath, value);
}

} // namespace Artifact
