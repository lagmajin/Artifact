module;
#include <utility>
#include <memory>
#include <wobjectdefs.h>
#include <QString>
#include <RefCntAutoPtr.hpp>
#include <Texture.h>
export module Artifact.Layer.EnvironmentMap;

import Artifact.Layer.Abstract;
import Property.Group;

export namespace Artifact {

// Environment Map Layer: holds an HDRI / cubemap used for skybox background and IBL lighting.
class ArtifactEnvironmentMapLayer : public ArtifactAbstractLayer {
    W_OBJECT(ArtifactEnvironmentMapLayer)
public:
    ArtifactEnvironmentMapLayer();
    virtual ~ArtifactEnvironmentMapLayer();

    // ArtifactAbstractLayer overrides
    void draw(ArtifactIRenderer* renderer) override;
    UniString className() const override { return "ArtifactEnvironmentMapLayer"; }
    bool is3D() const { return true; }
    bool isNullLayer() const override { return true; }
    bool shouldIncludeInFinalRender() const override { return false; }
    QRectF localBounds() const override;

    // Environment map properties
    QString hdriPath() const;
    void setHdriPath(const QString& path);

    float intensity() const;
    void setIntensity(float intensity);

    float rotation() const;
    void setRotation(float rotationDegrees);

    bool visibleAsBackground() const;
    void setVisibleAsBackground(bool visible);

    // Access the loaded cubemap texture
    Diligent::ITexture* cubemapTexture() const;
    void setCubemapTexture(Diligent::ITexture* texture);

    // Generic properties for Inspector
    std::vector<ArtifactCore::PropertyGroup> getLayerPropertyGroups() const override;
    bool setLayerPropertyValue(const QString& propertyPath, const QVariant& value) override;

private:
    struct Impl;
    Impl* envImpl_;
};

using ArtifactEnvironmentMapLayerPtr = std::shared_ptr<ArtifactEnvironmentMapLayer>;

} // namespace Artifact
