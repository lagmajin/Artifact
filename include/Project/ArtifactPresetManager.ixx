module;

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

export module Artifact.Project.PresetManager;

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



import Artifact.Effect.Abstract;
import Property.Abstract;
import Artifact.Color.Palette;

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPresetManager {
public:
    static bool saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath);
    static bool loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath);

    static QJsonObject effectToPresetJson(const ArtifactAbstractEffectPtr& effect);
    static bool applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json);

    // Color Palette
    static bool saveColorPaletteMapping(const ColorPaletteManager& manager, const QString& filePath);
    static bool loadColorPaletteMapping(ColorPaletteManager& manager, const QString& filePath);
};

} // namespace Artifact
