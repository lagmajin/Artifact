module;
#include <QVariant>
#include <QString>
#include <algorithm>
#include <vector>

export module Artifact.Effect.SurfaceFX;

import Artifact.Effect.Abstract;
import Graphics.Effect.SurfaceFX;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

class SurfaceFXEffect final : public ArtifactAbstractEffect {
public:
    SurfaceFXEffect() {
        setEffectID(UniString(QStringLiteral("surfacefx")));
        setDisplayName(UniString(QStringLiteral("SurfaceFX")));
        setPipelineStage(EffectPipelineStage::Rasterizer);
    }

    ArtifactCore::SurfaceFXData& data() noexcept { return data_; }
    const ArtifactCore::SurfaceFXData& data() const noexcept { return data_; }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override {
        std::vector<ArtifactCore::AbstractProperty> properties;
        ArtifactCore::AbstractProperty feather;
        feather.setName(QStringLiteral("Surface Feather"));
        feather.setType(ArtifactCore::PropertyType::Float);
        feather.setValue(data_.feather);
        properties.push_back(feather);
        return properties;
    }

    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override {
        if (name.toQString() == QStringLiteral("Surface Feather"))
            data_.feather = std::clamp(value.toFloat(), 0.0f, 1.0f);
    }

private:
    ArtifactCore::SurfaceFXData data_;
};

} // namespace Artifact
