module;
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
module Artifact.Core.SmartGuidesManager;
import std;
import Utils.Path;
import Artifact.Layer.Construction;
namespace Artifact {
QColor GuideSettings::colorForType(GuideType type) const {
    switch (type) {
    case GuideType::LayerEdge: return layerEdgeColor;
    case GuideType::LayerCenter: return layerCenterColor;
    case GuideType::CompEdge: return compEdgeColor;
    case GuideType::CompCenter: return compCenterColor;
    case GuideType::Spacing: return spacingColor;
    case GuideType::CustomGuide: return customGuideColor;
    case GuideType::SafeArea: return safeAreaColor;
    }
    return layerEdgeColor;
}
bool GuideSettings::isTypeEnabled(GuideType type) const {
    switch (type) {
    case GuideType::LayerEdge: return layerEdgesEnabled;
    case GuideType::LayerCenter: return layerCentersEnabled;
    case GuideType::CompEdge: return compEdgesEnabled;
    case GuideType::CompCenter: return compCentersEnabled;
    case GuideType::Spacing: return spacingEnabled;
    case GuideType::CustomGuide: return customGuidesEnabled;
    case GuideType::SafeArea: return safeAreasEnabled;
    }
    return true;
}
SmartGuidesManager* SmartGuidesManager::instance() {
    static SmartGuidesManager mgr;
    return &mgr;
}
const GuideSettings& SmartGuidesManager::settings() const { return settings_; }
void SmartGuidesManager::setSettings(const GuideSettings& s) { settings_ = s; }
void SmartGuidesManager::loadSettings() {
    QSettings s;
    s.beginGroup("SmartGuides");
    settings_.layerEdgesEnabled = s.value("layerEdgesEnabled", true).toBool();
    settings_.layerCentersEnabled = s.value("layerCentersEnabled", true).toBool();
    settings_.compEdgesEnabled = s.value("compEdgesEnabled", true).toBool();
    settings_.compCentersEnabled = s.value("compCentersEnabled", true).toBool();
    settings_.spacingEnabled = s.value("spacingEnabled", true).toBool();
    settings_.customGuidesEnabled = s.value("customGuidesEnabled", false).toBool();
    settings_.safeAreasEnabled = s.value("safeAreasEnabled", false).toBool();
    settings_.snapSensitivity = static_cast<float>(s.value("snapSensitivity", 10.0).toDouble());
    auto readColor = [&](const char* key, const QColor& def) {
        const QVariant v = s.value(key);
        return v.isValid() ? QColor(v.toString()) : def;
    };
    settings_.layerEdgeColor = readColor("layerEdgeColor", QColor(100, 180, 255, 180));
    settings_.layerCenterColor = readColor("layerCenterColor", QColor(100, 220, 160, 180));
    settings_.compEdgeColor = readColor("compEdgeColor", QColor(255, 200, 80, 180));
    settings_.compCenterColor = readColor("compCenterColor", QColor(255, 160, 80, 180));
    settings_.spacingColor = readColor("spacingColor", QColor(180, 140, 255, 180));
    settings_.customGuideColor = readColor("customGuideColor", QColor(255, 120, 120, 180));
    settings_.safeAreaColor = readColor("safeAreaColor", QColor(120, 255, 120, 160));
    s.endGroup();
}
void SmartGuidesManager::saveSettings() const {
    QSettings s;
    s.beginGroup("SmartGuides");
    s.setValue("layerEdgesEnabled", settings_.layerEdgesEnabled);
    s.setValue("layerCentersEnabled", settings_.layerCentersEnabled);
    s.setValue("compEdgesEnabled", settings_.compEdgesEnabled);
    s.setValue("compCentersEnabled", settings_.compCentersEnabled);
    s.setValue("spacingEnabled", settings_.spacingEnabled);
    s.setValue("customGuidesEnabled", settings_.customGuidesEnabled);
    s.setValue("safeAreasEnabled", settings_.safeAreasEnabled);
    s.setValue("snapSensitivity", static_cast<double>(settings_.snapSensitivity));
    auto writeColor = [&](const char* key, const QColor& c) {
        s.setValue(key, c.name(QColor::HexArgb));
    };
    writeColor("layerEdgeColor", settings_.layerEdgeColor);
    writeColor("layerCenterColor", settings_.layerCenterColor);
    writeColor("compEdgeColor", settings_.compEdgeColor);
    writeColor("compCenterColor", settings_.compCenterColor);
    writeColor("spacingColor", settings_.spacingColor);
    writeColor("customGuideColor", settings_.customGuideColor);
    writeColor("safeAreaColor", settings_.safeAreaColor);
    s.endGroup();
}
void SmartGuidesManager::buildCustomGuides(ArtifactCompositionPtr comp, std::vector<float>& outV, std::vector<float>& outH) {
    if (!comp || !settings_.customGuidesEnabled) return;
    const auto size = comp->effectiveCompositionSize();
    if (!size.isValid()) return;
    const auto layers = comp->allLayer();
    for (const auto& layer : layers) {
        if (!layer || !layer->isConstructionLayer()) {
            continue;
        }
        const auto constructionLayer =
            std::dynamic_pointer_cast<ArtifactConstructionLayer>(layer);
        if (!constructionLayer) {
            continue;
        }
        const auto guides = constructionLayer->constructionGuideSet().enabledGuides();
        for (const auto& guide : guides) {
            if (guide.orientation == GuideOrientation::Vertical) {
                outV.push_back(static_cast<float>(guide.position));
            } else if (guide.orientation == GuideOrientation::Horizontal) {
                outH.push_back(static_cast<float>(guide.position));
            }
        }
    }
}
}
