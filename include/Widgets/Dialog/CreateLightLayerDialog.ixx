module;

#include <QDialog>
#include <QString>
#include <QWidget>

export module Artifact.Widgets.CreateLightLayerDialog;

import Artifact.Layer.Light;
import Color.Float;

export namespace Artifact {

class CreateLightLayerDialog final : public QDialog {
public:
  explicit CreateLightLayerDialog(QWidget* parent = nullptr);
  ~CreateLightLayerDialog() override;

  [[nodiscard]] QString lightName() const;
  [[nodiscard]] LightType lightType() const;
  [[nodiscard]] ArtifactCore::FloatColor color() const;
  [[nodiscard]] float intensity() const;
  [[nodiscard]] float range() const;
  [[nodiscard]] float coneAngle() const;
  [[nodiscard]] float coneFeather() const;
  [[nodiscard]] float areaWidth() const;
  [[nodiscard]] float areaHeight() const;
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
