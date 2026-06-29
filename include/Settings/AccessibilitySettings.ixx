module;
#include <QColor>

export module Settings.Accessibility;

export namespace Artifact::Accessibility {

enum class Handedness { Right, Left };
enum class ColorDeficiencyMode { None, Protanopia, Deuteranopia, Tritanopia };

Handedness handedness();
bool        isLeftHanded();
bool        preferLargeTargets();
bool        preferHighContrastHints();
int         fontScalePercent();
ColorDeficiencyMode colorDeficiencyMode();
bool        reduceHoverDependency();

// scaling helpers — multiply base pixel sizes by these factors
float targetScale();   // ≥1.0 when preferLargeTargets
float contrastScale(); // ≥1.0 when preferHighContrastHints
float fontScale();     // fontScalePercent / 100.0

// convenience: scale an integer base size
inline int scaledSize(int base) {
    return static_cast<int>(static_cast<float>(base) * targetScale() + 0.5f);
}

// apply handedness to a context-menu popup position
void adjustContextMenuPosition(int& x, int& y, int menuWidth);

// convert a QColor through the active color deficiency filter
QColor adjustColorForDeficiency(const QColor& color);

} // namespace Artifact::Accessibility
