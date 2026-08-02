module;
#include <QVariant>
#include <QString>
#include <algorithm>
#include <cmath>
#include <vector>

export module Artifact.Effect.SurfaceFX;

import Artifact.Effect.Abstract;
import Graphics.Effect.SurfaceFX;
import Utils.String.UniString;
import Property.Abstract;

export namespace Artifact {

class SurfaceFXEffect final : public ArtifactAbstractEffect {
public:
    SurfaceFXEffect();

    ArtifactCore::SurfaceFXData& data() noexcept { return data_; }
    const ArtifactCore::SurfaceFXData& data() const noexcept { return data_; }

    std::vector<ArtifactCore::AbstractProperty> getProperties() const override {
        std::vector<ArtifactCore::AbstractProperty> properties;
        const auto addFloat = [&](const QString& name, float value) {
            ArtifactCore::AbstractProperty property;
            property.setName(name);
            property.setType(ArtifactCore::PropertyType::Float);
            property.setValue(value);
            properties.push_back(property);
        };
        ArtifactCore::AbstractProperty anchorType;
        anchorType.setName(QStringLiteral("Surface Anchor Type"));
        anchorType.setType(ArtifactCore::PropertyType::String);
        anchorType.setValue(anchorTypeName(data_.anchorType));
        properties.push_back(anchorType);
        ArtifactCore::AbstractProperty preset;
        preset.setName(QStringLiteral("Surface Preset"));
        preset.setType(ArtifactCore::PropertyType::String);
        preset.setValue(presetName_);
        properties.push_back(preset);
        addFloat(QStringLiteral("Surface Anchor X"), data_.anchorX);
        addFloat(QStringLiteral("Surface Anchor Y"), data_.anchorY);
        addFloat(QStringLiteral("Surface Anchor Width"), data_.anchorWidth);
        addFloat(QStringLiteral("Surface Anchor Height"), data_.anchorHeight);
        addFloat(QStringLiteral("Surface Feather"), data_.feather);

        ArtifactCore::AbstractProperty seed;
        seed.setName(QStringLiteral("Surface Field Seed"));
        seed.setType(ArtifactCore::PropertyType::Integer);
        seed.setValue(data_.fieldSeed);
        properties.push_back(seed);

        ArtifactCore::AbstractProperty elementCount;
        elementCount.setName(QStringLiteral("Surface Element Count"));
        elementCount.setType(ArtifactCore::PropertyType::Integer);
        elementCount.setValue(static_cast<int>(data_.elements.size()));
        properties.push_back(elementCount);
        if (!data_.elements.empty()) {
            const auto& element = data_.elements.front();
            ArtifactCore::AbstractProperty elementType;
            elementType.setName(QStringLiteral("Surface Element Type"));
            elementType.setType(ArtifactCore::PropertyType::String);
            elementType.setValue(elementTypeName(element.type));
            properties.push_back(elementType);
            addFloat(QStringLiteral("Surface Element X"), element.x);
            addFloat(QStringLiteral("Surface Element Y"), element.y);
            addFloat(QStringLiteral("Surface Element Width"), element.width);
            addFloat(QStringLiteral("Surface Element Height"), element.height);
            addFloat(QStringLiteral("Surface Element Rotation"), element.rotation);
            addFloat(QStringLiteral("Surface Element Intensity"), element.intensity);
            addFloat(QStringLiteral("Surface Element Opacity"), element.opacity);
            addFloat(QStringLiteral("Surface Element Roughness"), element.roughness);
            addFloat(QStringLiteral("Surface Element In Time"), element.inTime);
            addFloat(QStringLiteral("Surface Element Out Time"), element.outTime);
            ArtifactCore::AbstractProperty seedOffset;
            seedOffset.setName(QStringLiteral("Surface Element Seed Offset"));
            seedOffset.setType(ArtifactCore::PropertyType::Integer);
            seedOffset.setValue(element.seedOffset);
            properties.push_back(seedOffset);
        }
        return properties;
    }

    void setPropertyValue(const ArtifactCore::UniString& name, const QVariant& value) override {
        const QString propertyName = name.toQString();
        const auto finiteClamp = [](float input, float minimum, float maximum, float fallback) {
            return std::isfinite(input) ? std::clamp(input, minimum, maximum) : fallback;
        };
        if (propertyName == QStringLiteral("Surface Preset"))
            applyPreset(value.toString());
        else if (propertyName == QStringLiteral("Surface Anchor Type"))
            data_.anchorType = anchorTypeFromName(value.toString());
        else if (propertyName == QStringLiteral("Surface Anchor X"))
            data_.anchorX = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
        else if (propertyName == QStringLiteral("Surface Anchor Y"))
            data_.anchorY = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
        else if (propertyName == QStringLiteral("Surface Anchor Width"))
            data_.anchorWidth = finiteClamp(value.toFloat(), 0.0f, 1.0f, 1.0f);
        else if (propertyName == QStringLiteral("Surface Anchor Height"))
            data_.anchorHeight = finiteClamp(value.toFloat(), 0.0f, 1.0f, 1.0f);
        else if (propertyName == QStringLiteral("Surface Feather"))
            data_.feather = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
        else if (propertyName == QStringLiteral("Surface Field Seed"))
            data_.fieldSeed = value.toInt();
        else if (propertyName == QStringLiteral("Surface Element Count")) {
            const int targetCount = std::clamp(value.toInt(), 0, 128);
            const std::size_t oldCount = data_.elements.size();
            data_.elements.resize(static_cast<std::size_t>(targetCount));
            for (std::size_t index = oldCount; index < data_.elements.size(); ++index) {
                auto& element = data_.elements[index];
                element.id = QStringLiteral("surface-element-%1").arg(index + 1);
                element.seedOffset = static_cast<int>(index);
            }
        }
        else if (!data_.elements.empty()) {
            auto& element = data_.elements.front();
            if (propertyName == QStringLiteral("Surface Element Type"))
                element.type = elementTypeFromName(value.toString());
            else if (propertyName == QStringLiteral("Surface Element X"))
                element.x = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
            else if (propertyName == QStringLiteral("Surface Element Y"))
                element.y = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
            else if (propertyName == QStringLiteral("Surface Element Width"))
                element.width = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
            else if (propertyName == QStringLiteral("Surface Element Height"))
                element.height = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
            else if (propertyName == QStringLiteral("Surface Element Rotation"))
                element.rotation = std::isfinite(value.toFloat()) ? value.toFloat() : 0.0f;
            else if (propertyName == QStringLiteral("Surface Element Intensity"))
                element.intensity = finiteClamp(value.toFloat(), 0.0f, 1.0f, 1.0f);
            else if (propertyName == QStringLiteral("Surface Element Opacity"))
                element.opacity = finiteClamp(value.toFloat(), 0.0f, 1.0f, 1.0f);
            else if (propertyName == QStringLiteral("Surface Element Roughness"))
                element.roughness = finiteClamp(value.toFloat(), 0.0f, 1.0f, 0.0f);
            else if (propertyName == QStringLiteral("Surface Element In Time"))
                element.inTime = std::isfinite(value.toFloat()) ? value.toFloat() : 0.0f;
            else if (propertyName == QStringLiteral("Surface Element Out Time"))
                element.outTime = std::isfinite(value.toFloat()) ? value.toFloat() : -1.0f;
            else if (propertyName == QStringLiteral("Surface Element Seed Offset"))
                element.seedOffset = value.toInt();

        }

        // Keep the normalized anchor a valid rectangle after every edit,
        // including values restored from generic property editors.
        data_.anchorWidth = std::clamp(data_.anchorWidth, 0.0f, 1.0f - data_.anchorX);
        data_.anchorHeight = std::clamp(data_.anchorHeight, 0.0f, 1.0f - data_.anchorY);
        data_.anchorX = std::clamp(data_.anchorX, 0.0f, 1.0f);
        data_.anchorY = std::clamp(data_.anchorY, 0.0f, 1.0f);
        for (auto& element : data_.elements) {
            element.x = std::clamp(element.x, data_.anchorX,
                                   data_.anchorX + data_.anchorWidth);
            element.y = std::clamp(element.y, data_.anchorY,
                                   data_.anchorY + data_.anchorHeight);
            element.width = std::clamp(element.width, 0.0f,
                                       data_.anchorX + data_.anchorWidth - element.x);
            element.height = std::clamp(element.height, 0.0f,
                                        data_.anchorY + data_.anchorHeight - element.y);
            if (element.outTime >= 0.0f && element.outTime < element.inTime)
                element.outTime = element.inTime;
        }
        if (propertyName != QStringLiteral("Surface Preset"))
            presetName_ = QStringLiteral("custom");
        syncImpl();
    }

private:
    void syncImpl();

    void applyPreset(const QString& presetName) {
        const bool knownPreset =
            presetName == QStringLiteral("cameraLensFineScratches") ||
            presetName == QStringLiteral("cameraLensFingerSmudge") ||
            presetName == QStringLiteral("windowLightRain") ||
            presetName == QStringLiteral("windowHeavyRain") ||
            presetName == QStringLiteral("windowCondensation");
        if (!knownPreset)
            return;
        presetName_ = presetName;
        data_.elements.clear();
        auto addElement = [&](const QString& id,
                              ArtifactCore::SurfaceFXElementType type,
                              float x, float y, float width, float height,
                              float intensity, float opacity, int seedOffset) {
            ArtifactCore::SurfaceFXElement element;
            element.id = id;
            element.type = type;
            element.x = x;
            element.y = y;
            element.width = width;
            element.height = height;
            element.intensity = intensity;
            element.opacity = opacity;
            element.seedOffset = seedOffset;
            data_.elements.push_back(element);
        };

        if (presetName == QStringLiteral("cameraLensFineScratches")) {
            addElement(QStringLiteral("lens-scratch-1"), ArtifactCore::SurfaceFXElementType::Scratch,
                       0.35f, 0.42f, 0.28f, 0.012f, 0.65f, 0.75f, 1);
            addElement(QStringLiteral("lens-scratch-2"), ArtifactCore::SurfaceFXElementType::Scratch,
                       0.48f, 0.55f, 0.22f, 0.009f, 0.45f, 0.55f, 2);
        } else if (presetName == QStringLiteral("cameraLensFingerSmudge")) {
            addElement(QStringLiteral("lens-smudge"), ArtifactCore::SurfaceFXElementType::Dirt,
                       0.52f, 0.48f, 0.22f, 0.16f, 0.5f, 0.35f, 3);
        } else if (presetName == QStringLiteral("windowLightRain")) {
            addElement(QStringLiteral("rain-drop-1"), ArtifactCore::SurfaceFXElementType::Droplet,
                       0.42f, 0.18f, 0.035f, 0.06f, 0.65f, 0.55f, 4);
            addElement(QStringLiteral("rain-streak-1"), ArtifactCore::SurfaceFXElementType::Streak,
                       0.63f, 0.24f, 0.018f, 0.28f, 0.45f, 0.4f, 5);
        } else if (presetName == QStringLiteral("windowHeavyRain")) {
            for (int i = 0; i < 6; ++i) {
                addElement(QStringLiteral("heavy-rain-%1").arg(i + 1),
                           i % 2 == 0 ? ArtifactCore::SurfaceFXElementType::Droplet
                                      : ArtifactCore::SurfaceFXElementType::Streak,
                           0.18f + 0.13f * static_cast<float>(i),
                           0.12f + 0.09f * static_cast<float>(i % 3),
                           0.018f + 0.004f * static_cast<float>(i % 2),
                           0.08f + 0.05f * static_cast<float>(i % 3),
                           0.55f, 0.5f, 10 + i);
            }
        } else if (presetName == QStringLiteral("windowCondensation")) {
            addElement(QStringLiteral("condensation"), ArtifactCore::SurfaceFXElementType::Condensation,
                       0.5f, 0.5f, 0.9f, 0.8f, 0.35f, 0.3f, 20);
        }
    }

    static QString elementTypeName(ArtifactCore::SurfaceFXElementType type) {
        switch (type) {
        case ArtifactCore::SurfaceFXElementType::Droplet:
            return QStringLiteral("droplet");
        case ArtifactCore::SurfaceFXElementType::Streak:
            return QStringLiteral("streak");
        case ArtifactCore::SurfaceFXElementType::Condensation:
            return QStringLiteral("condensation");
        case ArtifactCore::SurfaceFXElementType::Dirt:
            return QStringLiteral("dirt");
        case ArtifactCore::SurfaceFXElementType::Scratch:
        default:
            return QStringLiteral("scratch");
        }
    }

    static ArtifactCore::SurfaceFXElementType elementTypeFromName(const QString& name) {
        if (name == QStringLiteral("droplet"))
            return ArtifactCore::SurfaceFXElementType::Droplet;
        if (name == QStringLiteral("streak"))
            return ArtifactCore::SurfaceFXElementType::Streak;
        if (name == QStringLiteral("condensation"))
            return ArtifactCore::SurfaceFXElementType::Condensation;
        if (name == QStringLiteral("dirt"))
            return ArtifactCore::SurfaceFXElementType::Dirt;
        return ArtifactCore::SurfaceFXElementType::Scratch;
    }

    static QString anchorTypeName(ArtifactCore::SurfaceFXAnchorType type) {
        switch (type) {
        case ArtifactCore::SurfaceFXAnchorType::Planar:
            return QStringLiteral("planar");
        case ArtifactCore::SurfaceFXAnchorType::TrackedPlanar:
            return QStringLiteral("trackedPlanar");
        case ArtifactCore::SurfaceFXAnchorType::WorldSurface:
            return QStringLiteral("worldSurface");
        case ArtifactCore::SurfaceFXAnchorType::ScreenSpace:
        default:
            return QStringLiteral("screenSpace");
        }
    }

    static ArtifactCore::SurfaceFXAnchorType anchorTypeFromName(const QString& name) {
        if (name == QStringLiteral("planar"))
            return ArtifactCore::SurfaceFXAnchorType::Planar;
        if (name == QStringLiteral("trackedPlanar"))
            return ArtifactCore::SurfaceFXAnchorType::TrackedPlanar;
        if (name == QStringLiteral("worldSurface"))
            return ArtifactCore::SurfaceFXAnchorType::WorldSurface;
        return ArtifactCore::SurfaceFXAnchorType::ScreenSpace;
    }

    QString presetName_ = QStringLiteral("custom");
    ArtifactCore::SurfaceFXData data_;
};

} // namespace Artifact
