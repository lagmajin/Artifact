module;

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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <any>
#include <atomic>
#include <queue>
#include <deque>
#include <list>
#include <tuple>
#include <numeric>
#include <regex>
#include <random>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
export module Artifact.Project.PresetManager;




import std;
import Artifact.Effect.Abstract;
import Artifact.Mask.LayerMask;
import Property.Abstract;
import Artifact.Color.Palette;

export namespace Artifact {

using namespace ArtifactCore;

enum class ArtifactCreationPresetKind { Composition, Layer };
enum class ArtifactCreationLayerKind { Solid, Image, Shape, Text };
enum class ArtifactCreationMaskKind { None, Circle, RoundedRectangle, Ellipse, Polygon };

struct ArtifactCreationPreset {
    QString id;
    QString displayName;
    QString description;
    ArtifactCreationPresetKind kind = ArtifactCreationPresetKind::Layer;
    ArtifactCreationLayerKind layerKind = ArtifactCreationLayerKind::Solid;
    ArtifactCreationMaskKind maskKind = ArtifactCreationMaskKind::None;
    int width = 1920;
    int height = 1080;
    float maskInset = 0.0f;
    QString backgroundColor = QStringLiteral("#00000000");
    double frameRate = 30.0;
    int durationFrames = 300;
    std::vector<ArtifactCreationPreset> layers;
    bool hasMask() const noexcept { return maskKind != ArtifactCreationMaskKind::None; }
};

class ArtifactPresetManager {
public:
    // Disconnected creation definitions; these do not mutate editor objects.
    static std::vector<ArtifactCreationPreset> standardCreationPresets();
    static std::optional<ArtifactCreationPreset> creationPreset(const QString& id);
    static QJsonObject creationPresetToJson(const ArtifactCreationPreset& preset);
    static std::optional<ArtifactCreationPreset> creationPresetFromJson(const QJsonObject& json);
    static bool isValidCreationPreset(const ArtifactCreationPreset& preset);
    static std::vector<ArtifactCreationPreset> layerCreationPlan(const ArtifactCreationPreset& preset);

    static bool saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath);
    static bool loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath);

    static QJsonObject effectToPresetJson(const ArtifactAbstractEffectPtr& effect);
    static bool applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json);

    static QJsonObject maskToPresetJson(const LayerMask& mask);
    static bool applyPresetJsonToMask(LayerMask& mask, const QJsonObject& json);
    static bool saveMaskPreset(const LayerMask& mask, const QString& filePath);
    static bool loadMaskPreset(LayerMask& mask, const QString& filePath);

    // Color Palette
    static bool saveColorPaletteMapping(const ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath);
    static bool loadColorPaletteMapping(ArtifactCore::Color::ColorPaletteManager& manager, const QString& filePath);
};

} // namespace Artifact
