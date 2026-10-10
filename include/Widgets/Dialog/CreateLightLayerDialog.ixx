module;

#include <QDialog>
#include <QString>
#include <QWidget>

export module Artifact.Widgets.CreateLightLayerDialog;

import Artifact.Layer.Light;
import Color.Float;
import Math.Vec;

export namespace Artifact {

class CreateLightLayerDialog final : public QDialog {
public:
  explicit CreateLightLayerDialog(QWidget* parent = nullptr);
  ~CreateLightLayerDialog() override;

  [[nodiscard]] QString lightName() const;
  [[nodiscard]] LightType lightType() const;
  [[nodiscard]] ArtifactCore::FloatColor color() const;
  [[nodiscard]] ArtifactCore::Units::Percent intensity() const;
  // Point / Area reach. Spot authors its reach through coneLength().
  [[nodiscard]] ArtifactCore::Units::Pixels range() const;
  [[nodiscard]] ArtifactCore::Units::Degrees coneAngle() const;
  // Spot-only reach. Spot has no Range: Cone Length is the single authored
  // "how far" value, shared by the gizmo and the shadow frustum far plane.
  [[nodiscard]] ArtifactCore::Units::Pixels coneLength() const;
  [[nodiscard]] ArtifactCore::Units::Degrees coneFeather() const;
  [[nodiscard]] ArtifactCore::Units::Pixels areaWidth() const;
  [[nodiscard]] ArtifactCore::Units::Pixels areaHeight() const;
  [[nodiscard]] AreaLightShape areaShape() const;
  [[nodiscard]] bool castsShadows() const;

  void selectLightType(LightType type);
  void chooseColor();
  void refreshPresentation();
  void applyTo(ArtifactLightLayer& layer) const;

private:
  class Impl;
  Impl* impl_ = nullptr;
};

} // namespace Artifact
