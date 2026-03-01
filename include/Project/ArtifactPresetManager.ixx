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

export namespace Artifact {

using namespace ArtifactCore;

class ArtifactPresetManager {
public:
    static bool saveEffectPreset(const ArtifactAbstractEffectPtr& effect, const QString& filePath);
    static bool loadEffectPreset(ArtifactAbstractEffectPtr& effect, const QString& filePath);

    static QJsonObject effectToPresetJson(const ArtifactAbstractEffectPtr& effect);
    static bool applyPresetJsonToEffect(ArtifactAbstractEffectPtr& effect, const QJsonObject& json);
};

} // namespace Artifact
