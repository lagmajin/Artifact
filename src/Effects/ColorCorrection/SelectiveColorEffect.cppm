module;
#include <algorithm>
#include <memory>
#include <vector>
#include <QVariant>

module SelectiveColorEffect;

import Artifact.Effect.Abstract;
import Artifact.Effect.ImplBase;
import ImageProcessing.ColorTransform.SelectiveColor;
import Image.ImageF32x4RGBAWithCache;
import Property.Abstract;
import Utils.String.UniString;

namespace Artifact {

class SelectiveColorEffectCPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SelectiveColorProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }
};

class SelectiveColorEffectGPUImpl : public ArtifactEffectImplBase {
public:
    ArtifactCore::SelectiveColorProcessor processor_;

    void applyCPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        dst = src;
        float* pixels = dst.image().rgba32fData();
        if (!pixels) {
            return;
        }
        const int width = dst.image().width();
        const int height = dst.image().height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float* pixel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4u;
                processor_.applyPixel(pixel[0], pixel[1], pixel[2]);
            }
        }
    }

    void applyGPU(const ImageF32x4RGBAWithCache& src, ImageF32x4RGBAWithCache& dst) override {
        applyCPU(src, dst);
    }
};

namespace {
constexpr std::pair<const char*, ArtifactCore::SelectiveColorGroup> kGroupNames[] = {
    {"Reds", ArtifactCore::SelectiveColorGroup::Reds},
    {"Yellows", ArtifactCore::SelectiveColorGroup::Yellows},
    {"Greens", ArtifactCore::SelectiveColorGroup::Greens},
    {"Cyans", ArtifactCore::SelectiveColorGroup::Cyans},
    {"Blues", ArtifactCore::SelectiveColorGroup::Blues},
    {"Magentas", ArtifactCore::SelectiveColorGroup::Magentas},
    {"Whites", ArtifactCore::SelectiveColorGroup::Whites},
    {"Neutrals", ArtifactCore::SelectiveColorGroup::Neutrals},
    {"Blacks", ArtifactCore::SelectiveColorGroup::Blacks},
};
}

SelectiveColorEffect::SelectiveColorEffect() {
    setEffectID(UniString("effect.colorcorrection.selectivecolor"));
    setDisplayName(UniString("Selective Color"));
    setPipelineStage(EffectPipelineStage::Rasterizer);
    setCPUImpl(std::make_shared<SelectiveColorEffectCPUImpl>());
    setGPUImpl(std::make_shared<SelectiveColorEffectGPUImpl>());
    setComputeMode(ComputeMode::AUTO);
    applyPreset(preset_);
    syncImpls();
}

SelectiveColorEffect::~SelectiveColorEffect() = default;

void SelectiveColorEffect::applyPreset(Preset preset) {
    preset_ = preset;
    switch (preset_) {
    case Preset::Neutral:
        settings_ = ArtifactCore::SelectiveColorSettings::neutral();
        break;
    case Preset::Warm:
        settings_ = ArtifactCore::SelectiveColorSettings::warm();
        break;
    case Preset::Cool:
        settings_ = ArtifactCore::SelectiveColorSettings::cool();
        break;
    case Preset::Vivid:
        settings_ = ArtifactCore::SelectiveColorSettings::vivid();
        break;
    case Preset::Film:
        settings_ = ArtifactCore::SelectiveColorSettings::film();
        break;
    case Preset::Custom:
    default:
        break;
    }
}

void SelectiveColorEffect::setPreset(int preset) {
    const int clamped = std::clamp(preset, 0, 5);
    applyPreset(static_cast<Preset>(clamped));
    syncImpls();
}

void SelectiveColorEffect::setStrength(float value) {
    preset_ = Preset::Custom;
    settings_.strength = std::clamp(value, 0.0f, 1.0f);
    syncImpls();
}

void SelectiveColorEffect::setRelativeMode(bool value) {
    preset_ = Preset::Custom;
    settings_.relativeMode = value;
    syncImpls();
}

void SelectiveColorEffect::setPreserveLuma(bool value) {
    preset_ = Preset::Custom;
    settings_.preserveLuma = value;
    syncImpls();
}

void SelectiveColorEffect::setAdjustment(ArtifactCore::SelectiveColorGroup group, float c, float m, float y, float k) {
    preset_ = Preset::Custom;
    auto& adj = settings_.groups[static_cast<size_t>(group)];
    adj.cyan = std::clamp(c, -1.0f, 1.0f);
    adj.magenta = std::clamp(m, -1.0f, 1.0f);
    adj.yellow = std::clamp(y, -1.0f, 1.0f);
    adj.black = std::clamp(k, -1.0f, 1.0f);
    syncImpls();
}

void SelectiveColorEffect::syncImpls() {
    if (auto* cpu = dynamic_cast<SelectiveColorEffectCPUImpl*>(cpuImpl().get())) {
        cpu->processor_.setSettings(settings_);
    }
    if (auto* gpu = dynamic_cast<SelectiveColorEffectGPUImpl*>(gpuImpl().get())) {
        gpu->processor_.setSettings(settings_);
    }
}

std::vector<AbstractProperty> SelectiveColorEffect::getProperties() const {
    std::vector<AbstractProperty> props;
    props.reserve(40);

    AbstractProperty presetProp;
    presetProp.setName("Preset");
    presetProp.setType(PropertyType::Integer);
    presetProp.setValue(preset());
    presetProp.setDisplayPriority(-40);
    props.push_back(presetProp);

    AbstractProperty strengthProp;
    strengthProp.setName("Strength");
    strengthProp.setType(PropertyType::Float);
    strengthProp.setValue(QVariant(static_cast<double>(settings_.strength)));
    strengthProp.setDisplayPriority(-30);
    props.push_back(strengthProp);

    AbstractProperty relativeProp;
    relativeProp.setName("Relative Mode");
    relativeProp.setType(PropertyType::Boolean);
    relativeProp.setValue(settings_.relativeMode);
    relativeProp.setDisplayPriority(-20);
    props.push_back(relativeProp);

    AbstractProperty preserveProp;
    preserveProp.setName("Preserve Luma");
    preserveProp.setType(PropertyType::Boolean);
    preserveProp.setValue(settings_.preserveLuma);
    preserveProp.setDisplayPriority(-10);
    props.push_back(preserveProp);

    auto addFloat = [&props](const QString& name, float value, int priority) {
        AbstractProperty prop;
        prop.setName(name);
        prop.setType(PropertyType::Float);
        prop.setValue(QVariant(static_cast<double>(value)));
        prop.setDisplayPriority(priority);
        props.push_back(prop);
    };

    int basePriority = 0;
    for (const auto& [groupName, group] : kGroupNames) {
        const auto& adj = settings_.groups[static_cast<size_t>(group)];
        addFloat(QStringLiteral("%1 Cyan").arg(groupName), adj.cyan, basePriority + 0);
        addFloat(QStringLiteral("%1 Magenta").arg(groupName), adj.magenta, basePriority + 1);
        addFloat(QStringLiteral("%1 Yellow").arg(groupName), adj.yellow, basePriority + 2);
        addFloat(QStringLiteral("%1 Black").arg(groupName), adj.black, basePriority + 3);
        basePriority += 10;
    }

    return props;
}

void SelectiveColorEffect::setPropertyValue(const UniString& name, const QVariant& value) {
    const QString key = name.toQString();
    if (key == QStringLiteral("Preset")) {
        setPreset(value.toInt());
        return;
    }
    if (key == QStringLiteral("Strength")) {
        setStrength(value.toFloat());
        return;
    }
    if (key == QStringLiteral("Relative Mode")) {
        setRelativeMode(value.toBool());
        return;
    }
    if (key == QStringLiteral("Preserve Luma")) {
        setPreserveLuma(value.toBool());
        return;
    }

    for (const auto& [groupName, group] : kGroupNames) {
        if (key == QStringLiteral("%1 Cyan").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, value.toFloat(), adj.magenta, adj.yellow, adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Magenta").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, value.toFloat(), adj.yellow, adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Yellow").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, adj.magenta, value.toFloat(), adj.black);
            return;
        }
        if (key == QStringLiteral("%1 Black").arg(groupName)) {
            const auto& adj = settings_.groups[static_cast<size_t>(group)];
            setAdjustment(group, adj.cyan, adj.magenta, adj.yellow, value.toFloat());
            return;
        }
    }
}

} // namespace Artifact
