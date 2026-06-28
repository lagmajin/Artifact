module;
#include <algorithm>
#include <QtGlobal>
#include <QString>

module Settings.Accessibility;

import Application.AppSettings;

namespace Artifact::Accessibility {

static ArtifactCore::ArtifactAppSettings* s() {
    return ArtifactCore::ArtifactAppSettings::instance();
}

Handedness handedness() {
    return s()->accessibilityHandedness() == QStringLiteral("left")
               ? Handedness::Left : Handedness::Right;
}

bool isLeftHanded() {
    return handedness() == Handedness::Left;
}

bool preferLargeTargets() {
    return s()->accessibilityPreferLargeTargets();
}

bool preferHighContrastHints() {
    return s()->accessibilityPreferHighContrastHints();
}

float targetScale() {
    return preferLargeTargets() ? 1.35f : 1.0f;
}

float contrastScale() {
    return preferHighContrastHints() ? 1.2f : 1.0f;
}

void adjustContextMenuPosition(int& x, int& y, int menuWidth) {
    if (isLeftHanded()) {
        x = std::max(0, x - menuWidth);
    }
    Q_UNUSED(y);
}

} // namespace Artifact::Accessibility
