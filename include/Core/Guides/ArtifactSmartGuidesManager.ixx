module;
#include <vector>
#include <QColor>
#include <QString>
export module Artifact.Core.SmartGuidesManager;
import Artifact.Composition.Abstract;
import Artifact.Widgets.TransformGizmo;
export namespace Artifact {
struct GuideSettings {
    bool layerEdgesEnabled = true;
    bool layerCentersEnabled = true;
    bool compEdgesEnabled = true;
    bool compCentersEnabled = true;
    bool spacingEnabled = true;
    bool customGuidesEnabled = false;
    bool safeAreasEnabled = false;
    QColor layerEdgeColor{100, 180, 255, 180};
    QColor layerCenterColor{100, 220, 160, 180};
    QColor compEdgeColor{255, 200, 80, 180};
    QColor compCenterColor{255, 160, 80, 180};
    QColor spacingColor{180, 140, 255, 180};
    QColor customGuideColor{255, 120, 120, 180};
    QColor safeAreaColor{120, 255, 120, 160};
    float snapSensitivity = 10.0f;
    QColor colorForType(GuideType type) const;
    bool isTypeEnabled(GuideType type) const;
};
export class SmartGuidesManager {
public:
    static SmartGuidesManager* instance();
    const GuideSettings& settings() const;
    void setSettings(const GuideSettings& s);
    void loadSettings();
    void saveSettings() const;
    void buildCustomGuides(ArtifactCompositionPtr comp, std::vector<float>& outV, std::vector<float>& outH);
private:
    SmartGuidesManager() = default;
    GuideSettings settings_;
};
}
