module;
#include <algorithm>
#include <QtGlobal>
#include <QString>
#include <QColor>
#include <cmath>

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

int fontScalePercent() {
    return s()->accessibilityFontScalePercent();
}

ColorDeficiencyMode colorDeficiencyMode() {
    auto mode = s()->accessibilityColorDeficiencyMode();
    if (mode == QStringLiteral("protanopia"))  return ColorDeficiencyMode::Protanopia;
    if (mode == QStringLiteral("deuteranopia")) return ColorDeficiencyMode::Deuteranopia;
    if (mode == QStringLiteral("tritanopia"))  return ColorDeficiencyMode::Tritanopia;
    return ColorDeficiencyMode::None;
}

bool reduceHoverDependency() {
    return s()->accessibilityReduceHoverDependency();
}

float targetScale() {
    return preferLargeTargets() ? 1.35f : 1.0f;
}

float contrastScale() {
    return preferHighContrastHints() ? 1.2f : 1.0f;
}

float fontScale() {
    return static_cast<float>(fontScalePercent()) / 100.0f;
}

void adjustContextMenuPosition(int& x, int& y, int menuWidth) {
    if (isLeftHanded()) {
        x = std::max(0, x - menuWidth);
    }
    Q_UNUSED(y);
}

// Simple color-blindness simulation / correction matrices (LMS → dichromat)
// Based on the Brettel-Vienot-Mollon model simplified for real-time use.
static void applyProtanopia(float& r, float& g, float& b) {
    // lose L-cone sensitivity
    float nr = 0.0f;
    float ng = 0.494207f * r + 0.504000f * g + 0.001793f * b;
    float nb = 0.0f * r + 0.0f * g + 1.0f * b;
    r = nr; g = ng; b = nb;
}

static void applyDeuteranopia(float& r, float& g, float& b) {
    // lose M-cone sensitivity
    float nr = 0.0f;
    float ng = 0.0f;
    float nb = -0.100781f * r + 1.162696f * g + -0.061815f * b;
    r = nr; g = ng; b = nb;
    // Simplified: shift green toward blue
    float sr = 0.625f * r + 0.375f * g;
    float sg = 0.7f * g;
    float sb = b;
    r = sr; g = sg; b = sb;
}

static void applyTritanopia(float& r, float& g, float& b) {
    // lose S-cone sensitivity
    float nr = 0.95f * r + 0.05f * g;
    float ng = 0.0f * r + 1.0f * g + 0.0f * b;
    float nb = 0.0f;
    r = nr; g = ng; b = nb;
}

QColor adjustColorForDeficiency(const QColor& color) {
    auto mode = colorDeficiencyMode();
    if (mode == ColorDeficiencyMode::None) return color;

    float r = color.redF();
    float g = color.greenF();
    float b = color.blueF();

    switch (mode) {
    case ColorDeficiencyMode::Protanopia:  applyProtanopia(r, g, b);  break;
    case ColorDeficiencyMode::Deuteranopia: applyDeuteranopia(r, g, b); break;
    case ColorDeficiencyMode::Tritanopia:  applyTritanopia(r, g, b);  break;
    default: break;
    }

    r = std::clamp(r, 0.0f, 1.0f);
    g = std::clamp(g, 0.0f, 1.0f);
    b = std::clamp(b, 0.0f, 1.0f);

    return QColor::fromRgbF(r, g, b);
}

} // namespace Artifact::Accessibility
