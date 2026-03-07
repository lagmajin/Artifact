module;

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <memory>
#include <algorithm>
#include <cmath>
#include <functional>
#include <optional>
#include <utility>
#include <array>
#include <mutex>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
module Artifact.Project.PresetManager;




import Artifact.Effect.Abstract;
import Property.Abstract;
import Utils.String.UniString;
import Artifact.Color.Palette;

namespace Artifact {

QJsonObject ArtifactPresetManager::effectToPresetJson(const ArtifactAbstractEffectPtr& effect) {
    if (!effect) return {};

    QJsonObject root;
    root["effect_id"] = effect->effectID().toQString();
    root["display_name"] = effect->displayName().toQString();

    QJsonArray propsArray;
    auto props = effect->getProperties();
    for (const auto& p : props) {
        QJsonObject propObj;
        propObj["name"] = p.getName();
        propObj["type"] = static_cast<int>(p.getType());
        propObj["value"] = QJsonValue::fromVariant(p.getValue());
        propsArray.append(propObj);
    }
    root["properties"] = propsArray;
    return root;
}

bool ArtifactPresetManager::applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json) {
    if (!effect || json.isEmpty()) return false;

    // Check effect ID match? Or just apply what we can.
    QJsonArray propsArray = json["properties"].toArray();
    for (int i = 0; i < propsArray.size(); ++i) {
        QJsonObject propObj = propsArray[i].toObject();
        QString name = propObj["name"].toString();
        QVariant value = propObj["value"].toVariant();
        
        effect->setPropertyValue(UniString(name.toStdString()), value);
    }
    return true;
}

bool ArtifactPresetManager::saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath) {
    QJsonObject json = effectToPresetJson(effect);
    if (json.isEmpty()) return false;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QJsonDocument doc(json);
    file.write(doc.toJson());
    file.close();
    return true;
}

bool ArtifactPresetManager::loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) return false;

    return applyPresetJsonToEffect(effect, doc.object());
}

bool ArtifactPresetManager::saveColorPaletteMapping(const ColorPaletteManager& manager, const QString& filePath) {
    return manager.saveToFile(filePath);
}

bool ArtifactPresetManager::loadColorPaletteMapping(ColorPaletteManager& manager, const QString& filePath) {
    return manager.loadFromFile(filePath);
}

} // namespace Artifact
