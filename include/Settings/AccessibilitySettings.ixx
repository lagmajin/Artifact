export module Settings.Accessibility;

namespace Artifact::Accessibility {

enum class Handedness { Right, Left };

Handedness handedness();
bool        isLeftHanded();
bool        preferLargeTargets();
bool        preferHighContrastHints();

// scaling helpers — multiply base pixel sizes by these factors
float targetScale();   // ≥1.0 when preferLargeTargets
float contrastScale(); // ≥1.0 when preferHighContrastHints

// convenience: scale an integer base size
inline int scaledSize(int base) {
    return static_cast<int>(static_cast<float>(base) * targetScale() + 0.5f);
}

// apply handedness to a context-menu popup position
void adjustContextMenuPosition(int& x, int& y, int menuWidth);

} // namespace Artifact::Accessibility
