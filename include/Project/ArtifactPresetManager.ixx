module;

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>

export module Artifact.Project.PresetManager;

import std;
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
