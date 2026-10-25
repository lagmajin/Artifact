module;

#include <QString>
#include <QColor>

module Artifact.Project.Manager;

import Application.AppSettings;
import Artifact.Composition.InitParams;
import Color.Float;

namespace Artifact {

QString applicationCreationDefaultsJson()
{
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
        return settings->creationDefaultsJson();
    }
    return {};
}

void applyApplicationCompositionDefaults(
    ArtifactCompositionInitParams& params)
{
    if (auto* settings = ArtifactCore::ArtifactAppSettings::instance()) {
        params.setResolution(settings->projectDefaultCompositionWidth(),
                             settings->projectDefaultCompositionHeight());
        params.setFrameRate(settings->projectDefaultCompositionFrameRate());
        const QColor background(
            settings->projectDefaultCompositionBackgroundColor());
        if (background.isValid()) {
            params.setBackgroundColor(ArtifactCore::FloatColor(
                background.redF(), background.greenF(), background.blueF(),
                background.alphaF()));
        }
    }
}

} // namespace Artifact
