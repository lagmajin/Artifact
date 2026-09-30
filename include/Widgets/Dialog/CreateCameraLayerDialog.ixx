module;
#include <utility>

#include <QDialog>
#include <QWidget>
#include <wobjectdefs.h>
export module Artifact.Widgets.CreateCameraLayerDialog;

import Artifact.Layer.Camera;
import Math.Vec;

export namespace Artifact {

// Keep the dialog interface IFC synchronized with its implementation unit.
/// カメラ設定ダイアログ
/// docs/image/カメラレイヤー.jpeg に基づく UI
class CreateCameraLayerDialog final : public QDialog {
    W_OBJECT(CreateCameraLayerDialog)

public:
    explicit CreateCameraLayerDialog(QWidget* parent = nullptr);
    ~CreateCameraLayerDialog();

    // Returned settings
    QString cameraName() const;
    ArtifactCore::Units::Millimeters focalLength() const;
    ArtifactCore::Units::Degrees fov() const;
    ArtifactCore::Units::Pixels zoom() const;
    ArtifactCore::Units::Pixels focusDistance() const;
    ArtifactCore::Units::Percent blurAmount() const;
    ArtifactCore::Units::FStop apertureF() const;
    bool    depthOfFieldEnabled() const;
    bool    motionBlur() const;
    bool    cameraLocked() const;

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    class Impl;
    Impl* impl_;
};

} // namespace Artifact
